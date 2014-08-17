#include "transactiondescdialog.h"
#include "ui_transactiondescdialog.h"

#include "transactiontablemodel.h"

#include <QModelIndex>

TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
    setStyleSheet("background: url(:/images/res/images/dialogBackground.jpg); background-attachment: fixed; color: #ffaa00; selection-background-color: rgb(255, 170, 0); font-family: Plantagenet Cherokee; font-size: 14px; selection-color: white;");
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
