// Copyright (c) 2026 The NetCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef SPLITCOINSDIALOG_H
#define SPLITCOINSDIALOG_H

#include <QDialog>

class WalletModel;
class QLabel;
class QSpinBox;
class QPushButton;

/**
 * Splits the spendable balance into several coins of equal size, sent to
 * new addresses of this wallet. A stake locks only the coin(s) it uses
 * until it matures, so with several coins the rest stays spendable.
 */
class SplitCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SplitCoinsDialog(WalletModel *model, QWidget *parent = 0);

private slots:
    void updateInfo();
    void split();

private:
    WalletModel *model;
    QLabel *labelBalance;
    QLabel *labelCoins;
    QLabel *labelPart;
    QSpinBox *spinParts;
    QPushButton *buttonSplit;

    int countSpendableCoins() const;
    qint64 estimateFee(int nParts) const;
    qint64 partAmount(int nParts) const;
};

#endif // SPLITCOINSDIALOG_H
