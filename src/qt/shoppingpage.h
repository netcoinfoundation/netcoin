#ifndef SHOPPINGPAGE_H
#define SHOPPINGPAGE_H

#include "clientmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include <QWidget>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QSettings>
#include <QSlider>

#include <QDialog>

double getBlockHardness(int);
double getTxTotalValue(std::string);
double convertCoins(int64_t);
double getTxFees(std::string);
int getBlockTime(int);
int getBlocknBits(int);
int getBlockNonce(int);
int blocksInPastHours(int);
int getBlockHashrate(int);
std::string getInputs(std::string);
std::string getOutputs(std::string);
std::string getBlockHash(int);
std::string getBlockMerkle(int);
bool addnode(std::string);
const CBlockIndex* getBlockIndex(int);
int64_t getInputValue(CTransaction, CScript);

namespace Ui {
    class ShoppingPage;
}
class OptionsModel;

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

/** Widget that shows a list of sending or receiving addresses.
  */
class ShoppingPage : public QDialog
{
    Q_OBJECT

public:
       explicit ShoppingPage(QWidget *parent = 0);
    ~ShoppingPage();

       void setModel(ClientModel *model);

public slots:

       void blockClicked();
       void txClicked();
       void updateExplorer(bool);

private:
    Ui::ShoppingPage *ui;

    ClientModel *model;
    QString returnValue;
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;

private slots:

    void on_excButton_1_clicked();
    void on_excButton_2_clicked();
    void on_excButton_3_clicked();
    void on_excButton_4_clicked();
    void on_excButton_5_clicked();
//    void on_excButton_6_clicked();
//    void on_excButton_7_clicked();
//    void on_excButton_8_clicked();
      void on_servButton_1_clicked();
//    void on_servButton_2_clicked();
//    void on_servButton_3_clicked();
//    void on_servButton_4_clicked();
//    void on_servButton_5_clicked();
//    void on_servButton_6_clicked();
//    void on_servButton_7_clicked();
//    void on_servButton_8_clicked();
//    void on_servButton_9_clicked();
//    void on_servButton_10_clicked();
//    void on_servButton_11_clicked();
//    void on_servButton_12_clicked();
    void on_usefulButton_1_clicked();
    void on_usefulButton_2_clicked();
    void on_usefulButton_3_clicked();
    void on_usefulButton_4_clicked();
    void on_usefulButton_5_clicked();
    void on_usefulButton_6_clicked();

    void on_poolButton_1_clicked();
    void on_poolButton_2_clicked();
    void on_poolButton_3_clicked();
    void on_poolButton_4_clicked();
    void on_poolButton_5_clicked();
    void on_poolButton_6_clicked();


signals:

};

#endif // ADDRESSBOOKDIALOG_H
