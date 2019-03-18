#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "walletmodel.h"
#include "clientmodel.h"

#include "bitcoinunits.h"
#include "init.h"
#include "base58.h"
#include "bitcoingui.h"
#include "calcdialog.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "wallet.h"
#include "bitcoinrpc.h"
#include "askpassphrasedialog.h"
#include <algorithm>

#include "main.h"
#include "util.h"
#include <QAbstractItemDelegate>
#include <QPainter>
#include <QIcon>
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QFrame>
#include <sstream>
#include <string>
#include <QMenu>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

using namespace json_spirit;

extern CWallet* pwalletMain;
extern int64_t nLastCoinStakeSearchInterval;
double GetPoSKernelPS();
double GetPoSKernelPS2(const CBlockIndex* pindex);
class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));
        painter->setPen(QColor(204,204,204));
        painter->drawLine(QPoint(mainRect.left(), mainRect.bottom()), QPoint(mainRect.right(), mainRect.bottom()));
        painter->drawLine(QPoint(amountRect.left()-6, mainRect.top()), QPoint(amountRect.left()-6, mainRect.bottom()));
        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    currentWeight(0),
    currentNetworkWeight(0),
    txdelegate(new TxViewDelegate()),
    filter(0)
{

    ui->setupUi(this);

    if (GetBoolArg("-staking", true))
    {
        QTimer *timerMyWeight = new QTimer();
        connect(timerMyWeight, SIGNAL(timeout()), this, SLOT(updateMyWeight()));
        timerMyWeight->start(30 * 1000);
        updateMyWeight();
    }
    QAction *stakeForCharityAction = new QAction(ui->startButton->text(), this);

    contextMenu = new QMenu();
    contextMenu->addAction(stakeForCharityAction);
    connect(stakeForCharityAction, SIGNAL(triggered()), this, SLOT(on_startButton_clicked()));

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings

    showOutOfSyncWarning(true);

        // setup Plot
        // create graph
        ui->diffplot->addGraph();

        // give the axes some labels:
        ui->diffplot->xAxis->setLabel("Block Height");

        // set the pens

        ui->diffplot->graph(0)->setLineStyle(QCPGraph::lsLine);

        // set axes label fonts:
        QFont label = font();
        ui->diffplot->xAxis->setLabelFont(label);
        ui->diffplot->yAxis->setLabelFont(label);
         ui->diffplot->setBackground(Qt::transparent);
         ui->diffplot->setStyleSheet("background:transparent");
         ui->diffplot->axisRect()->setBackground(QColor(0, 0, 0,0));
             ui->diffplot->xAxis->setLabelColor("#0381ff");
          ui->diffplot->yAxis->setLabelColor("#0381ff");
 ui->scrollArea->setStyleSheet("background:transparent");
}



void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}



void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    // int unit = model->getOptionsModel()->getDisplayUnit();
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
    ui->labelStake->setText(BitcoinUnits::formatWithUnit(unit, stake));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(BitcoinUnits::formatWithUnit(unit, immatureBalance));
    ui->labelTotal->setText(BitcoinUnits::formatWithUnit(unit, balance + stake + unconfirmedBalance + immatureBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);

    // qint64 interest = model->getTransactionTableModel()->getInterestGenerated();
    qint64 interest = walletModel->getTransactionTableModel()->getInterestGenerated();
     // Setup display values for 'interest summary' widget at bottom right
    ui->labelInterest->setText(BitcoinUnits::formatWithUnit(unit, interest));
}

void OverviewPage::setNumTransactions(int count)
{
    ui->labelNumTransactions->setText(QLocale::system().toString(count));
}


void OverviewPage::updateMyWeight()
{
    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    if (nLastCoinStakeSearchInterval && pwalletMain && !IsInitialBlockDownload()) //netcoin GetStakeWeight requires mutex lock on wallet which tends to freeze initial block downloads
        pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

    if (nLastCoinStakeSearchInterval && nWeight)
    {
        uint64_t nNetworkWeight = GetPoSKernelPS();
        unsigned nEstimateTime = nTargetSpacing * 2 * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }

        ui->labelMyWeight->setText(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br>Expected time to earn reward is %3").arg(nWeight).arg(nNetworkWeight).arg(text));
    }
    else
    {
        if (pwalletMain && pwalletMain->IsLocked())
            ui->labelMyWeight->setText(tr("Not staking because your wallet is locked,<br> please unlock for staking."));
        else if (vNodes.empty())
            ui->labelMyWeight->setText(tr("Not staking because your wallet is offline,<br> please wait for a connection..."));
        else if (IsInitialBlockDownload())
            ui->labelMyWeight->setText(tr("Not staking because your wallet is syncing,<br> please wait for this process to end..."));
        else if (!nWeight)
            ui->labelMyWeight->setText(tr("Not staking because you don't have mature coins..."));
        else
            ui->labelMyWeight->setText(tr("Not staking"));
    }
}
void OverviewPage::on_chartState_clicked()
{
    if (ui->diffplotframe->isHidden()){
ui->chartState->setText("disable chart");
ui->diffplotframe->setHidden(false);
updateGraph();

    }else{
            ui->diffplotframe->setHidden(true);
            ui->chartState->setText("enable chart");


}
}
void OverviewPage::on_powpos_clicked()
{
    if (ui->powpos->isChecked()){
ui->powpos->setText("pos");
updateGraph();

  }else{

            ui->powpos->setText("pow");
            updateGraph();
}
}
void OverviewPage::on_toggle10_clicked()
{
 numLookBack=10;
 updateGraph();
}

void OverviewPage::on_toggle100_clicked()
{
  numLookBack=100;
 updateGraph();
}
void OverviewPage::on_toggle1000_clicked()
{
 numLookBack=1000;
 updateGraph();
}
void OverviewPage::on_toggleall_clicked()
{
 numLookBack=heightPrevious;
 updateGraph();

}
void OverviewPage::on_startButton_clicked()
{
    return stakeForCharitySignal();
}

// void OverviewPage::setModel(WalletModel *model)
void OverviewPage::setClientModel(ClientModel *model)
{
    // this->model = model;
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();

    // update statistics
    updateStatistics();
    //set up a timer to auto refresh every 30 seconds to update the statistics
    QTimer *timerNetworkStats = new QTimer();
    connect(timerNetworkStats, SIGNAL(timeout()), this, SLOT(updateStatistics()));
    timerNetworkStats->start(30 * 1000);

    // Unlock wallet button
    WalletModel::EncryptionStatus status = model->getEncryptionStatus();
    if(status == WalletModel::Unencrypted)
    {
        ui->unlockWalletActionNew->setDisabled(true);
    }
    connect(ui->unlockWalletActionNew, SIGNAL(clicked()), this, SLOT(lockWalletToggle()));
}

void OverviewPage::updateDisplayUnit()
{
    // if(model && model->getOptionsModel())
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            // setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);
            setBalance(currentBalance, walletModel->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
        // txdelegate->unit = model->getOptionsModel()->getDisplayUnit();
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);

}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::lockWalletToggle()
{
    // if(model->getEncryptionStatus() == WalletModel::Locked)
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog dlg(AskPassphraseDialog::UnlockStaking, this);
        // dlg.setModel(model);
        dlg.setModel(walletModel);
        if(dlg.exec() == QDialog::Accepted)
        {
            ui->unlockWalletActionNew->setText("Lock Wallet");
        }
    }
    else
    {
        // model->setWalletLocked(true);
        walletModel->setWalletLocked(true);
        ui->unlockWalletActionNew->setText("Unlock Wallet");
    }
}
int init=0;
void OverviewPage::updateGraph(){

    if (init==0){
            numLookBack=10;

     init=1;
}

if (!ui->diffplotframe->isHidden()){

    if(fDebug) { printf("Plot: Getting Ready: pindexBest: %p\n", pindexBest); }

        bool fProofOfStake = (ui->powpos->isChecked());
    if (fProofOfStake)
        ui->diffplot->yAxis->setLabel("24H Network Weight");
        else
        ui->diffplot->yAxis->setLabel("Difficulty");
     double diffMax = 0;
    const CBlockIndex* pindex = GetLastBlockIndex(pindexBest, fProofOfStake);
    int height = pindex->nHeight;
    int xStart = std::max<int>( height-numLookBack , 0) + 1;
    int xEnd = height;

    // Start at the end and walk backwards
    int i = numLookBack-1;
    int x = xEnd;

    // This should be a noop if the size is already 2000
    vX.resize(numLookBack);
    vY.resize(numLookBack);

    if(fDebug) {
        if(height != pindex->nHeight) {
            printf("Plot: Warning: nBestHeight and pindexBest->nHeight don't match: %d:%d:\n", height, pindex->nHeight);
        }
    }

    if(fDebug) { printf("Plot: Reading blockchain\n"); }

    const CBlockIndex* itr = pindex;
    while(i >= 0 && itr != NULL)
    {
        if(fDebug) { printf("Plot: Processing block: %d - pprev: %p\n", itr->nHeight, itr->pprev); }
        vX[i] = itr->nHeight;
        if (itr->nHeight < xStart) {
            xStart = itr->nHeight;
        }
        vY[i] = fProofOfStake ?  GetPoSKernelPS2(itr) : GetDifficulty(itr);
        diffMax = std::max<double>(diffMax, vY[i]);

        itr = GetLastBlockIndex(itr->pprev, fProofOfStake);
        i--;
        x--;
    }

    if(fDebug) { printf("Plot: Drawing plot\n"); }

    ui->diffplot->graph(0)->setData(vX,vY);

    // set axes ranges, so we see all data:
    ui->diffplot->xAxis->setRange((double)xStart, (double)xEnd);
    ui->diffplot->yAxis->setRange(0, diffMax+(diffMax/10));

    ui->diffplot->xAxis->setAutoSubTicks(false);
    ui->diffplot->yAxis->setAutoSubTicks(false);
    ui->diffplot->xAxis->setSubTickCount(0);
    ui->diffplot->yAxis->setSubTickCount(0);
    ui->diffplot->xAxis->setLabelColor("#0381ff");
 ui->diffplot->yAxis->setLabelColor("#0381ff");
 ui->diffplot->xAxis->setTickLabelColor("#0381ff");
ui->diffplot->yAxis->setTickLabelColor("#0381ff");

ui->diffplot->xAxis2->setTickLabelColor("#0381ff");
    if(fDebug) { printf("Plot: Done!\n"); }
    ui->diffplot->replot();


}
}
void OverviewPage::updateStatistics()
{
    CBlockIndex* pindexCheckpoint;

pindexCheckpoint = mapBlockIndex[Checkpoints::hashSyncCheckpoint];
    double pHardness = GetDifficulty();
    double pHardness2 = GetDifficulty(GetLastBlockIndex(pindexBest, true));
    int pPawrate = GetPoWMHashPS();
    double pPawrate2 = 0.000;
    int nHeight = pindexBest->nHeight;
    int ncheckpointHeight = pindexCheckpoint->nHeight;
    double nSubsidy = double (GetProofOfWorkReward(nHeight, 0, pindexBest->GetBlockHash())/ double (COIN));
    uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);
    uint64_t nNetworkWeight = GetPoSKernelPS();
    int64_t volume = ((pindexBest->nMoneySupply)/100000000);
    int peers = this->modelStatistics->getNumConnections();
    pPawrate2 = (double)pPawrate;
    QString height = QString::number(nHeight);
    QString checkpointheight = QString::number(ncheckpointHeight);
    QString subsidy = QString::number(nSubsidy, 'f', 6);
    QString hardness = QString::number(pHardness, 'f', 6);
    QString hardness2 = QString::number(pHardness2, 'f', 6);
    QString pawrate = QString::number(pPawrate2, 'f', 3);
    QString Qlpawrate = modelStatistics->getLastBlockDate().toString();
    QPixmap cpconnect (":/icons/connect_4");
    QPixmap cpnoconnect (":/icons/connect_0");
    QString QPeers = QString::number(peers);
    QString qVolume = QLocale::system().toString((qlonglong)volume);

    if(nHeight > heightPrevious && !IsInitialBlockDownload())
    {
        ui->heightBox->setText("<b><font color=\"green\">" + height + "</font></b>");
        static int64_t lastUpdate = 0;
        // Double Check to make sure we don't try to update the plot when it is disabled

    }
    else
    {
        if(!IsInitialBlockDownload())
        ui->heightBox->setText(height);
                else
                ui->heightBox->setText("downloading");
    }

    if(nSubsidy < rewardPrevious)
    {
        ui->rewardBox->setText("<b><font color=\"red\">" + subsidy + "</font></b>");
    }
    else
    {
        ui->rewardBox->setText(subsidy);
    }

    if(pHardness > hardnessPrevious)
    {
        ui->diffBox->setText("<b><font color=\"green\">" + hardness + "</font></b>");
    }
    else if(pHardness < hardnessPrevious)
    {
        ui->diffBox->setText("<b><font color=\"red\">" + hardness + "</font></b>");
    }
    else
    {
        ui->diffBox->setText(hardness);
    }
    if(nNetworkWeight > stakemaxPrevious)
    {
        ui->diffplot->graph(0)->setPen(QPen(QColor("#00bd09")));

    }
    else if(nNetworkWeight < stakemaxPrevious)
    {

        ui->diffplot->graph(0)->setPen(QPen(QColor("#ff0015")));
    }
    else
    {
           ui->diffplot->graph(0)->setPen(QPen(QColor("#00bd09")));
    }
    if(pHardness2 > hardnessPrevious2)
    {
        ui->diffBox2->setText("<b><font color=\"green\">" + hardness2 + "</font></b>");

    }
    else if(pHardness2 < hardnessPrevious2)
    {

        ui->diffBox2->setText("<b><font color=\"red\">" + hardness2 + "</font></b>");
    }
    else
    {
        ui->diffBox2->setText(hardness2);
    }

    if(pPawrate2 > netPawratePrevious)
    {
        ui->pawrateBox->setText("<b><font color=\"green\">" + pawrate + " MH/s</font></b>");
    }
    else if(pPawrate2 < netPawratePrevious)
    {
        ui->pawrateBox->setText("<b><font color=\"red\">" + pawrate + " MH/s</font></b>");
    }
    else
    {
        ui->pawrateBox->setText(pawrate + " MH/s");
    }

    if(Qlpawrate != pawratePrevious)
    {
        ui->localBox->setText("<b><font color=\"green\">" + Qlpawrate + "</font></b>");
    }
    else
    {
    ui->localBox->setText(Qlpawrate);
    }

    if(peers > connectionPrevious)
    {
        ui->connectionBox->setText("<b><font color=\"green\">" + QPeers + "</font></b>");
    }
    else if(peers < connectionPrevious)
    {
        ui->connectionBox->setText("<b><font color=\"red\">" + QPeers + "</font></b>");
    }
    else
    {
        ui->connectionBox->setText(QPeers);
    }

    if(ncheckpointHeight >= (nHeight-71))
    {
        ui->labelcheckpointHeight->setText("<b><font color=\"green\">" + checkpointheight + "</font></b>");
         ui->labelcheckpointConnect->setPixmap(cpconnect);

    }
    else {
        ui->labelcheckpointHeight->setText("<b><font color=\"red\">" + checkpointheight + "</font></b>");
           ui->labelcheckpointConnect->setPixmap(cpnoconnect);


    }

    if(volume > volumePrevious)
    {
        ui->volumeBox->setText("<b>" + qVolume + " NET" + "</font></b>");
    }
    else if(volume < volumePrevious)
    {
        ui->volumeBox->setText("<b>" + qVolume + " NET" + "</font></b>");
    }
    else
    {
        ui->volumeBox->setText(qVolume + " NET");
    }

       //update graph
       updateGraph();
    updatePrevious(nHeight, nMinWeight, nNetworkWeight, nSubsidy, pHardness, pHardness2, pPawrate2, Qlpawrate, peers, volume,ncheckpointHeight);
}

void OverviewPage::updatePrevious(int nHeight, int nMinWeight, int nNetworkWeight, double nSubsidy, double pHardness, double pHardness2, double pPawrate2, QString Qlpawrate, int peers, int volume,int ncheckpointHeight)
{
    heightPrevious = nHeight;
    stakeminPrevious = nMinWeight;
    stakemaxPrevious = nNetworkWeight;
    rewardPrevious = nSubsidy;
    hardnessPrevious = pHardness;
    hardnessPrevious2 = pHardness2;
    netPawratePrevious = pPawrate2;
    pawratePrevious = Qlpawrate;
    connectionPrevious = peers;
    volumePrevious = volume;
    checkpointPrevious =ncheckpointHeight;
}

void OverviewPage::setStatistics(ClientModel *modelStatistics)
{
    updateStatistics();
    this->modelStatistics = modelStatistics;

}

OverviewPage::~OverviewPage()
{
    delete ui;
}
