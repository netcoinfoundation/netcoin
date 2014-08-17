#include "networkpage.h"
#include "ui_networkpage.h"

#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

NetworkPage::NetworkPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NetworkPage)

{
    ui->setupUi(this);
}

NetworkPage::~NetworkPage()
{
    delete ui;
}
