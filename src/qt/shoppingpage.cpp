#include "shoppingpage.h"
#include "ui_shoppingpage.h"

#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QScrollArea>
#include <QMenu>
#include <QUrl>
#include <QDesktopServices>
#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

ShoppingPage::ShoppingPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShoppingPage)
{
    ui->setupUi(this);

}

ShoppingPage::~ShoppingPage()
{
    delete ui;
}


//Exchange Links
void ShoppingPage::on_pushButton_36_clicked()
{
    QString link="https://www.cryptsy.com/markets/view/134";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_39_clicked()
{
    QString link="https://www.cryptsy.com/markets/view/108";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_37_clicked()
{
    QString link="https://bter.com/trade/net_cny";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_38_clicked()
{
    QString link="https://bter.com/trade/net_ltc";
    QDesktopServices::openUrl(QUrl(link));
}


//Shopping Links
void ShoppingPage::on_pushButton_40_clicked()
{
    QString link="http://alpenpowerpro.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_43_clicked()
{
    QString link="http://btcpipeshop.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_46_clicked()
{
    QString link="http://bitezze.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_41_clicked()
{
    QString link="http://www.cryptodirect.cf/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_44_clicked()
{
    QString link="http://www.grkreationsdirect.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_47_clicked()
{
    QString link="http://bitezze.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_42_clicked()
{
    QString link="http://www.mintagemastermind.com/index.html";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_45_clicked()
{
    QString link="http://www.picca.co.uk/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_48_clicked()
{
    QString link="http://www.retrotowers.co.uk/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_79_clicked()
{
    QString link="http://scryptstore.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_78_clicked()
{
    QString link="http://zetasteam.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_49_clicked()
{
    QString link="http://thestakingmachine.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_81_clicked()
{
    QString link="http://tuffwraps.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_80_clicked()
{
    QString link="http://waterido.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_82_clicked()
{
    QString link="http://wrol.info/survival-gear-bitcoin/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_53_clicked()
{
    QString link="http://residencialcastor.com.br/";
    QDesktopServices::openUrl(QUrl(link));
}

//Gambling Links
void ShoppingPage::on_pushButton_50_clicked()
{
    QString link="http://ecasino.io/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_51_clicked()
{
    QString link="http://lottery.netcoinfoundation.org/";
    QDesktopServices::openUrl(QUrl(link));
}

//Charity Links
void ShoppingPage::on_pushButton_57_clicked()
{
    QString link="http://www.songsoflove.org/netcoin/";
    QDesktopServices::openUrl(QUrl(link));
}

//Useful links tab
void ShoppingPage::on_pushButton_55_clicked()
{
    QString link="http://forum.netcoinfoundation.org/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_58_clicked()
{
    QString link="http://explorer.netcoinfoundation.org/";
    QDesktopServices::openUrl(QUrl(link));
}

void ShoppingPage::on_pushButton_56_clicked()
{
    QString link="http://paperwallet.netcoinfoundation.org/";
    QDesktopServices::openUrl(QUrl(link));
}

//Development Site links
void ShoppingPage::on_pushButton_clicked()
{
    QString link="http://development.netcoinfoundation.org/";
    QDesktopServices::openUrl(QUrl(link));
}
