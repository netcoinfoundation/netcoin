// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "splitcoinsdialog.h"

#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include "init.h"
#include "main.h"
#include "wallet.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SplitCoinsDialog::SplitCoinsDialog(WalletModel *model, QWidget *parent) :
    QDialog(parent), model(model)
{
    setWindowTitle(tr("Split coins"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *info = new QLabel(tr("A stake locks the coin it uses, together with the reward, until it matures (50 blocks). "
                                 "If your whole balance is one coin, nothing can be sent during that time.\n\n"
                                 "This splits your spendable balance into several equal coins on new addresses of this wallet. "
                                 "A stake then locks only one of them, the others stay spendable and keep staking. "
                                 "The coins do not leave your wallet; only the network fee is paid."), this);
    info->setWordWrap(true);
    layout->addWidget(info);

    labelBalance = new QLabel(this);
    labelCoins = new QLabel(this);
    layout->addWidget(labelBalance);
    layout->addWidget(labelCoins);

    QHBoxLayout *layoutParts = new QHBoxLayout();
    QLabel *labelParts = new QLabel(tr("Number of &coins:"), this);
    spinParts = new QSpinBox(this);
    spinParts->setRange(2, 50);
    spinParts->setValue(5);
    labelParts->setBuddy(spinParts);
    layoutParts->addWidget(labelParts);
    layoutParts->addWidget(spinParts);
    layoutParts->addStretch();
    layout->addLayout(layoutParts);

    labelPart = new QLabel(this);
    labelPart->setWordWrap(true);
    layout->addWidget(labelPart);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    buttonSplit = buttons->addButton(tr("&Split"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("&Close"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(spinParts, SIGNAL(valueChanged(int)), this, SLOT(updateInfo()));
    connect(buttonSplit, SIGNAL(clicked()), this, SLOT(split()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    updateInfo();
    setMinimumWidth(560);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    adjustSize();
}

int SplitCoinsDialog::countSpendableCoins() const
{
    std::vector<COutput> vCoins;
    pwalletMain->AvailableCoins(vCoins);
    return vCoins.size();
}

qint64 SplitCoinsDialog::estimateFee(int nParts) const
{
    // generous estimate: ~180 bytes per input, 34 per output (+ change)
    int nBytes = 180 * std::max(1, countSpendableCoins()) + 34 * (nParts + 1) + 10;
    qint64 nFeePerKb = std::max<qint64>(MIN_TX_FEE, nTransactionFee);
    return 2 * nFeePerKb * (1 + nBytes / 1000);
}

qint64 SplitCoinsDialog::partAmount(int nParts) const
{
    qint64 nAvailable = model->getBalance() - estimateFee(nParts);
    if (nAvailable <= 0)
        return 0;
    return (nAvailable / nParts / CENT) * CENT;
}

void SplitCoinsDialog::updateInfo()
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    labelBalance->setText(tr("Spendable now: %1").arg(BitcoinUnits::formatWithUnit(unit, model->getBalance())));
    labelCoins->setText(tr("Spendable coins in the wallet now: %1").arg(countSpendableCoins()));

    int nParts = spinParts->value();
    qint64 nPart = partAmount(nParts);
    bool fOk = nPart >= COIN;
    buttonSplit->setEnabled(fOk);
    if (fOk)
        labelPart->setText(tr("Each coin: %1. The network fee is at most about %2; what is left over comes back as one more small coin.")
                           .arg(BitcoinUnits::formatWithUnit(unit, nPart),
                                BitcoinUnits::formatWithUnit(unit, estimateFee(nParts))));
    else
        labelPart->setText(tr("The spendable balance is too small to split into this many coins."));
}

void SplitCoinsDialog::split()
{
    int unit = model->getOptionsModel()->getDisplayUnit();
    int nParts = spinParts->value();
    qint64 nPart = partAmount(nParts);
    if (nPart < COIN)
        return;

    if (QMessageBox::question(this, tr("Split coins"),
            tr("Split %1 into %2 coins of %3 on new addresses of this wallet?")
                .arg(BitcoinUnits::formatWithUnit(unit, model->getBalance()))
                .arg(nParts)
                .arg(BitcoinUnits::formatWithUnit(unit, nPart)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid())
        return;

    QList<SendCoinsRecipient> recipients;
    for (int i = 0; i < nParts; i++)
    {
        SendCoinsRecipient rcp;
        rcp.label = tr("Split coins");
        rcp.address = model->getAddressTableModel()->addRow(AddressTableModel::Receive, rcp.label, "");
        rcp.amount = nPart;
        if (rcp.address.isEmpty())
        {
            QMessageBox::warning(this, tr("Split coins"), tr("Could not create a new address."));
            return;
        }
        recipients.append(rcp);
    }

    WalletModel::SendCoinsReturn ret = model->sendCoins(recipients);
    if (ret.status == WalletModel::AmountWithFeeExceedsBalance && ret.fee > 0)
    {
        // the fee turned out higher than estimated: make the parts smaller
        qint64 nSmaller = ((model->getBalance() - 2 * ret.fee) / nParts / CENT) * CENT;
        if (nSmaller >= COIN)
        {
            for (int i = 0; i < recipients.size(); i++)
                recipients[i].amount = nSmaller;
            ret = model->sendCoins(recipients);
        }
    }

    switch (ret.status)
    {
    case WalletModel::OK:
        QMessageBox::information(this, tr("Split coins"),
            tr("Done. The new coins can be used for staking and sending once the transaction is confirmed."));
        accept();
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(this, tr("Split coins"),
            tr("The total exceeds your balance when the %1 transaction fee is included.")
                .arg(BitcoinUnits::formatWithUnit(unit, ret.fee)));
        break;
    case WalletModel::Aborted:
        break;
    default:
        QMessageBox::warning(this, tr("Split coins"), tr("Error: Transaction creation failed."));
        break;
    }
}
