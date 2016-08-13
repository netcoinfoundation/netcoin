#include "calcdialog.h"
#include "ui_calcdialog.h"

#include "clientmodel.h"
#include "notificator.h"
#include "version.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"

#include "walletmodel.h"

#include "wallet.h"
#include "walletdb.h"
#include "main.h"
#include "init.h"

#include <algorithm>
#include <iomanip>
#include <QString>

calcDialog::calcDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::calcDialog)
{
    ui->setupUi(this);
	
	connect(ui->pushButton, SIGNAL(clicked()), this, SLOT(pushButtonClicked()));
}

calcDialog::~calcDialog()
{
    delete ui;
}

void calcDialog::setModel(ClientModel *model)
{

}

double min(double var1, double var2)
{
    return (var1 > var2 ? var2 : var1);
}

void calcDialog::pushButtonClicked()
    {
         QString strUserSize = ui->blockSizeEdit->text();
        double dUserBlock = strUserSize.toDouble();
        double dReward = 0;

        if(dUserBlock >= 0 && dUserBlock < 1000) {
        dReward  = (((dUserBlock - 0.0) / 1000.0) * 5 + 20);
        }
        else
        if(dUserBlock >= 1000 && dUserBlock < 10000) {
        dReward  = (((dUserBlock - 1000.0) / 9000.0) * 5 + 25);
        }
        else
        if(dUserBlock >= 10000 && dUserBlock < 100000) {
        dReward  = (((dUserBlock - 10000.0) / 90000.0) * 10 + 30);
        }
        else
        if(dUserBlock >= 100000 && dUserBlock < 1000000) {
        dReward  = (((dUserBlock - 100000.0) / 900000.0) * 50 + 35);
        }
        else
        if(dUserBlock >= 1000000) {
        dReward  = (min(((dUserBlock - 1000000.0) / 9000000.0), 1) * 5 + 40);
        }
        ui->yearlyAPR->setText(QString::number(dReward));
        ui->dayResult->setText(QString::number(dReward / 36500.0 * dUserBlock));
    }

void calcDialog::on_buttonBox_accepted()
{
	close();
}
