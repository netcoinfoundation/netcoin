#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "bitcoinunits.h"
#include "monitoreddatamapper.h"
#include "netbase.h"
#include "optionsmodel.h"
#include "conffile.h"
#include "net.h"
#include "torcontrol.h"
#include "util.h"
#include "wallet.h"

#include <QCheckBox>
#include <QKeyEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QRegularExpressionValidator>

OptionsDialog::OptionsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(0),
    mapper(0),
    fRestartWarningDisplayed_Proxy(false),
    fRestartWarningDisplayed_Lang(false),
    fProxyIpValid(true),
    fStakeCombineLoaded(false),
    fNetConfChanged(false),
    fListenOnionLoaded(true)
{
    ui->setupUi(this);
    setupNetConfWidgets();

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    ui->socksVersion->setEnabled(false);
    ui->socksVersion->addItem("5", 5);
    ui->socksVersion->addItem("4", 4);
    ui->socksVersion->setCurrentIndex(0);

    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyIp, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyPort, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->socksVersion, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning_Proxy()));

    ui->proxyIp->installEventFilter(this);

    /* Window elements init */
#ifdef Q_OS_MAC
    ui->tabWindow->setVisible(false);
#endif

    /* Display elements init */
    QDir translations(":translations");
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    foreach(const QString &langStr, translations.entryList())
    {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language - country (locale name)", e.g. "German - Germany (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" - ") + QLocale::countryToString(locale.country()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
        else
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language (locale name)", e.g. "German (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
    }

    ui->unit->setModel(new BitcoinUnits(this));

    /* Widget-to-option mapper */
    mapper = new MonitoredDataMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* enable apply button when data modified */
    connect(mapper, SIGNAL(viewModified()), this, SLOT(enableApplyButton()));
    /* disable apply button when new data loaded */
    connect(mapper, SIGNAL(currentIndexChanged(int)), this, SLOT(disableApplyButton()));
    /* setup/change UI elements when proxy IP is invalid/valid */
    connect(this, SIGNAL(proxyIpValid(QValidatedLineEdit *, bool)), this, SLOT(handleProxyIpValid(QValidatedLineEdit *, bool)));
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setModel(OptionsModel *model)
{
    this->model = model;

    if(model)
    {
        connect(model, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        mapper->setModel(model);
        setMapper();
        mapper->toFirst();
    }

    /* update the display unit, to not use the default ("BTC") */
    updateDisplayUnit();

    /* warn only when language selection changes by user action (placed here so init via mapper doesn't trigger this) */
    connect(ui->lang, SIGNAL(valueChanged()), this, SLOT(showRestartWarning_Lang()));

    /* disable apply button after settings are loaded as there is nothing to save */
    disableApplyButton();
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->transactionFee, OptionsModel::Fee);
    mapper->addMapping(ui->reserveBalance, OptionsModel::ReserveBalance);
    mapper->addMapping(ui->bitcoinAtStartup, OptionsModel::StartAtStartup);
    // mapper->addMapping(ui->detachDatabases, OptionsModel::DetachDatabases);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);
    mapper->addMapping(ui->socksVersion, OptionsModel::ProxySocksVersion);

    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->displayAddresses, OptionsModel::DisplayAddresses);
    mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);
}

void OptionsDialog::enableApplyButton()
{
    ui->applyButton->setEnabled(true);
}

void OptionsDialog::disableApplyButton()
{
    ui->applyButton->setEnabled(false);
}

void OptionsDialog::enableSaveButtons()
{
    /* prevent enabling of the save buttons when data modified, if there is an invalid proxy address present */
    if(fProxyIpValid)
        setSaveButtonState(true);
}

void OptionsDialog::disableSaveButtons()
{
    setSaveButtonState(false);
}

void OptionsDialog::setSaveButtonState(bool fState)
{
    ui->applyButton->setEnabled(fState);
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_okButton_clicked()
{
    if (!saveNetConf())
        return;
    mapper->submit();
    accept();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_applyButton_clicked()
{
    if (!saveNetConf())
        return;
    mapper->submit();
    disableApplyButton();
}

void OptionsDialog::showRestartWarning_Proxy()
{
    if(!fRestartWarningDisplayed_Proxy)
    {
        QMessageBox::warning(this, tr("Warning"), tr("This setting will take effect after restarting Netcoin."), QMessageBox::Ok);
        fRestartWarningDisplayed_Proxy = true;
    }
}

void OptionsDialog::showRestartWarning_Lang()
{
    if(!fRestartWarningDisplayed_Lang)
    {
        QMessageBox::warning(this, tr("Warning"), tr("This setting will take effect after restarting Netcoin."), QMessageBox::Ok);
        fRestartWarningDisplayed_Lang = true;
    }
}

void OptionsDialog::updateDisplayUnit()
{
    if(model)
    {
        /* Update transactionFee with the current unit */
        ui->transactionFee->setDisplayUnit(model->getDisplayUnit());
    }
}

void OptionsDialog::handleProxyIpValid(QValidatedLineEdit *object, bool fState)
{
    // this is used in a check before re-enabling the save buttons
    fProxyIpValid = fState;

    if(fProxyIpValid)
    {
        enableSaveButtons();
        ui->statusLabel->clear();
    }
    else
    {
        disableSaveButtons();
        object->setValid(fProxyIpValid);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

bool OptionsDialog::eventFilter(QObject *object, QEvent *event)
{
    if (object == addNodeEdit && event->type() == QEvent::KeyPress)
    {
        int nKey = static_cast<QKeyEvent*>(event)->key();
        if (nKey == Qt::Key_Return || nKey == Qt::Key_Enter)
        {
            addNode();
            return true;
        }
    }
    if(event->type() == QEvent::FocusOut)
    {
        if(object == ui->proxyIp)
        {
            CService addr;
            /* Check proxyIp for a valid IPv4/IPv6 address and emit the proxyIpValid signal */
            emit proxyIpValid(ui->proxyIp, LookupNumeric(ui->proxyIp->text().toStdString().c_str(), addr));
        }
    }
    return QDialog::eventFilter(object, event);
}

/*
 * Tor and addnode settings. These are core options read from netcoin.conf
 * (shared with netcoind), so they are edited there and not in QSettings.
 */
void OptionsDialog::setupNetConfWidgets()
{
    /* Main tab: combining coins in stakes */
    stakeCombine = new QCheckBox(tr("&Combine coins when staking"), this);
    stakeCombine->setToolTip(tr("When on, a stake also takes in other coins of the same address. All of them are then locked until the stake matures (50 blocks). When off, only the coin that found the stake is locked and the rest of the balance stays spendable."));
    ui->verticalLayout_Main->insertWidget(ui->verticalLayout_Main->count() - 1, stakeCombine);
    connect(stakeCombine, SIGNAL(toggled(bool)), this, SLOT(netConfChanged()));

    QVBoxLayout *layout = ui->verticalLayout_Network;
    int nPos = layout->count() - 1; // before the vertical spacer

    /* Tor */
    QGroupBox *boxTor = new QGroupBox(tr("Tor onion service"), this);
    QVBoxLayout *layoutTor = new QVBoxLayout(boxTor);
    listenOnion = new QCheckBox(tr("Make this node reachable through &Tor (works behind NAT and CGNAT)"), boxTor);
    listenOnion->setToolTip(tr("Creates a .onion address for this node while Tor is running. Other NetCoin 3.1+ nodes can then connect to you even if your router does not allow incoming connections."));
    layoutTor->addWidget(listenOnion);
    QHBoxLayout *layoutControl = new QHBoxLayout();
    QLabel *labelControl = new QLabel(tr("Tor &control port:"), boxTor);
    torControl = new QLineEdit(boxTor);
    torControl->setPlaceholderText(DEFAULT_TOR_CONTROL);
    torControl->setMaximumWidth(160);
    labelControl->setBuddy(torControl);
    layoutControl->addWidget(labelControl);
    layoutControl->addWidget(torControl);
    layoutControl->addStretch();
    layoutTor->addLayout(layoutControl);
    QLabel *hintTor = new QLabel(tr("Tor Browser: 127.0.0.1:9151 (keep it open), Tor service: 127.0.0.1:9051"), boxTor);
    hintTor->setWordWrap(true);
    layoutTor->addWidget(hintTor);
    onionStatus = new QLabel(boxTor);
    onionStatus->setWordWrap(true);
    onionStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layoutTor->addWidget(onionStatus);
    layout->insertWidget(nPos++, boxTor);

    /* addnode */
    QGroupBox *boxNodes = new QGroupBox(tr("Always connect to these nodes"), this);
    QVBoxLayout *layoutNodes = new QVBoxLayout(boxNodes);
    addNodeList = new QListWidget(boxNodes);
    addNodeList->setMaximumHeight(110);
    addNodeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layoutNodes->addWidget(addNodeList);
    QHBoxLayout *layoutAdd = new QHBoxLayout();
    addNodeEdit = new QLineEdit(boxNodes);
    addNodeEdit->setPlaceholderText(tr("IP address, host name or .onion, optionally with :port"));
    addNodeButton = new QPushButton(tr("&Add"), boxNodes);
    addNodeButton->setAutoDefault(false);
    removeNodeButton = new QPushButton(tr("&Remove"), boxNodes);
    removeNodeButton->setAutoDefault(false);
    removeNodeButton->setEnabled(false);
    layoutAdd->addWidget(addNodeEdit);
    layoutAdd->addWidget(addNodeButton);
    layoutAdd->addWidget(removeNodeButton);
    layoutNodes->addLayout(layoutAdd);
    QLabel *hintNodes = new QLabel(tr("Saved in netcoin.conf (addnode=). New nodes are connected right away; Tor settings take effect after restarting NetCoin."), boxNodes);
    hintNodes->setWordWrap(true);
    layoutNodes->addWidget(hintNodes);
    layout->insertWidget(nPos++, boxNodes);

    loadNetConf();

    connect(listenOnion, SIGNAL(toggled(bool)), torControl, SLOT(setEnabled(bool)));
    connect(listenOnion, SIGNAL(toggled(bool)), this, SLOT(netConfChanged()));
    connect(torControl, SIGNAL(textEdited(QString)), this, SLOT(netConfChanged()));
    connect(addNodeButton, SIGNAL(clicked()), this, SLOT(addNode()));
    addNodeEdit->installEventFilter(this); // Enter adds the node instead of closing the dialog
    connect(removeNodeButton, SIGNAL(clicked()), this, SLOT(removeNode()));
    connect(addNodeList, SIGNAL(itemSelectionChanged()), this, SLOT(netConfChanged()));

    updateOnionStatus();
    resize(qMax(width(), 640), qMax(height(), 620));
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateOnionStatus()));
    timer->start(2000);
}

void OptionsDialog::loadNetConf()
{
    std::vector<std::string> v = ConfFile::GetValues("stakecombine");
    fStakeCombineLoaded = v.empty() ? false : (atoi(v.back().c_str()) != 0);
    stakeCombine->blockSignals(true);
    stakeCombine->setChecked(fStakeCombineLoaded);
    stakeCombine->blockSignals(false);

    v = ConfFile::GetValues("listenonion");
    fListenOnionLoaded = v.empty() ? DEFAULT_LISTEN_ONION : (atoi(v.back().c_str()) != 0);
    v = ConfFile::GetValues("torcontrol");
    strTorControlLoaded = v.empty() ? QString() : QString::fromStdString(v.back());
    listAddNodesLoaded.clear();
    for (const std::string& strNode : ConfFile::GetValues("addnode"))
        listAddNodesLoaded << QString::fromStdString(strNode);

    listenOnion->setChecked(fListenOnionLoaded);
    torControl->setEnabled(fListenOnionLoaded);
    torControl->setText(strTorControlLoaded);
    addNodeList->clear();
    addNodeList->addItems(listAddNodesLoaded);
    fNetConfChanged = false;
}

void OptionsDialog::netConfChanged()
{
    removeNodeButton->setEnabled(!addNodeList->selectedItems().isEmpty());
    if (sender() == addNodeList)
        return; // only the selection changed
    fNetConfChanged = true;
    enableApplyButton();
}

void OptionsDialog::addNode()
{
    QString strNode = addNodeEdit->text().trimmed();
    if (strNode.isEmpty())
        return;
    int nPort = 0;
    std::string strHost;
    SplitHostPort(strNode.toStdString(), nPort, strHost);
    if (strHost.empty() || strNode.contains(' ') || strNode.contains('#') || strNode.contains('=')) {
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("\"%1\" is not a valid node address.").arg(strNode));
        return;
    }
    ui->statusLabel->clear();
    if (addNodeList->findItems(strNode, Qt::MatchFixedString).isEmpty()) {
        addNodeList->addItem(strNode);
        netConfChanged();
    }
    addNodeEdit->clear();
}

void OptionsDialog::removeNode()
{
    foreach (QListWidgetItem *item, addNodeList->selectedItems())
        delete item;
    netConfChanged();
}

void OptionsDialog::updateOnionStatus()
{
    std::string strOnion = GetOnionServiceAddress();
    if (!strOnion.empty() && IsTorNetworkDisabled())
        onionStatus->setText(tr("Tor is running but not connected to the Tor network, so %1 cannot be reached. In the Tor Browser click \"Connect\".").arg(QString::fromStdString(strOnion)));
    else if (!strOnion.empty())
        onionStatus->setText(tr("This node is reachable as %1").arg(QString::fromStdString(strOnion)));
    else if (GetBoolArg("-listenonion", DEFAULT_LISTEN_ONION) && !fNoListen)
        onionStatus->setText(tr("No onion address yet: Tor is not running or not reachable (see debug.log)."));
    else
        onionStatus->setText(tr("Tor onion service is switched off."));
}

bool OptionsDialog::saveNetConf()
{
    if (!fNetConfChanged)
        return true;

    QStringList listNodes;
    for (int i = 0; i < addNodeList->count(); i++)
        listNodes << addNodeList->item(i)->text();
    QString strControl = torControl->text().trimmed();

    std::map<std::string, std::vector<std::string> > mapValues;
    std::vector<std::string>& vNodes = mapValues["addnode"];
    foreach (const QString& strNode, listNodes)
        vNodes.push_back(strNode.toStdString());
    if (stakeCombine->isChecked() != fStakeCombineLoaded || !ConfFile::GetValues("stakecombine").empty())
        mapValues["stakecombine"].push_back(stakeCombine->isChecked() ? "1" : "0");
    if (listenOnion->isChecked() != fListenOnionLoaded || !ConfFile::GetValues("listenonion").empty())
        mapValues["listenonion"].push_back(listenOnion->isChecked() ? "1" : "0");
    if (strControl != strTorControlLoaded) {
        mapValues["torcontrol"];
        if (!strControl.isEmpty())
            mapValues["torcontrol"].push_back(strControl.toStdString());
    }

    std::string strError;
    if (!ConfFile::SetValues(mapValues, strError)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot save the settings to netcoin.conf: %1").arg(QString::fromStdString(strError)));
        return false;
    }

    /* takes effect right away */
    fStakeCombine = stakeCombine->isChecked();

    /* addnode takes effect right away */
    {
        LOCK(cs_vAddedNodes);
        foreach (const QString& strNode, listAddNodesLoaded)
            if (!listNodes.contains(strNode)) {
                std::vector<std::string>::iterator it = std::find(vAddedNodes.begin(), vAddedNodes.end(), strNode.toStdString());
                if (it != vAddedNodes.end())
                    vAddedNodes.erase(it);
            }
        foreach (const QString& strNode, listNodes)
            if (std::find(vAddedNodes.begin(), vAddedNodes.end(), strNode.toStdString()) == vAddedNodes.end())
                vAddedNodes.push_back(strNode.toStdString());
    }

    bool fTorChanged = listenOnion->isChecked() != fListenOnionLoaded || strControl != strTorControlLoaded;
    if (fTorChanged)
        QMessageBox::information(this, tr("Tor"), tr("The Tor settings will take effect after restarting NetCoin."), QMessageBox::Ok);

    loadNetConf();
    return true;
}
