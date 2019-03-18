#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include "clientmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include "qcustomplot.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QSettings>
#include <QSlider>
#include "qcustomplot.h"

QT_BEGIN_NAMESPACE
class QModelIndex;

QT_END_NAMESPACE

namespace Ui {
    class OverviewPage;
}
class ClientModel;
class WalletModel;
class TxViewDelegate;
class TransactionFilterProxy;
class QLabel;
class QMenu;
class QFrame;
class QHBoxLayout;

/** Overview ("home") page widget **/
class OverviewPage : public QWidget
{
    Q_OBJECT
public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);

    int heightPrevious;
    int connectionPrevious;
    int volumePrevious;
    int stakeminPrevious;
    int stakemaxPrevious;
    int numLookBack;
    int checkpointPrevious;
    QString stakePrevious;
    double rewardPrevious;
    double netPawratePrevious;
    QString pawratePrevious;
    double hardnessPrevious;
    double hardnessPrevious2;

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setNumTransactions(int count);
    void setStatistics(ClientModel *modelStatistics);
    void lockWalletToggle();
    void updateStatistics();
    void updatePrevious(int, int, int, double, double, double, double, QString, int, int,int);


signals:
    void transactionClicked(const QModelIndex &index);

private:
    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    CWallet *wallet;
    ClientModel *modelStatistics;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 interest;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;
    QMenu *contextMenu;

    	QVector<double> vX;
    	QVector<double> vY;

    //Weight label
    qint64 currentWeight;
    qint64 currentNetworkWeight;
    qint64 currentMyWeight;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;

private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void updateAlerts(const QString &warnings);
    void updateMyWeight();
    void on_startButton_clicked();
    void on_chartState_clicked();
    void on_toggle10_clicked();
    void on_toggle100_clicked();
    void on_toggle1000_clicked();
    void on_toggleall_clicked();
    void on_powpos_clicked();
    void updateGraph();


signals:
    void stakeForCharitySignal();
};

#endif // OVERVIEWPAGE_H
