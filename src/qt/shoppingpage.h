#ifndef SHOPPINGPAGE_H
#define SHOPPINGPAGE_H

#include <QDialog>

namespace Ui {
    class ShoppingPage;
}
class AddressTableModel;
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

public slots:


private:
    Ui::ShoppingPage *ui;

    QString returnValue;
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;

private slots:

    void on_pushButton_36_clicked();

    void on_pushButton_39_clicked();

    void on_pushButton_37_clicked();

    void on_pushButton_38_clicked();

    void on_pushButton_40_clicked();

    void on_pushButton_43_clicked();

    void on_pushButton_46_clicked();

    void on_pushButton_41_clicked();

    void on_pushButton_44_clicked();

    void on_pushButton_47_clicked();

    void on_pushButton_42_clicked();

    void on_pushButton_45_clicked();

    void on_pushButton_48_clicked();

    void on_pushButton_79_clicked();

    void on_pushButton_78_clicked();

    void on_pushButton_49_clicked();

    void on_pushButton_81_clicked();

    void on_pushButton_80_clicked();

    void on_pushButton_82_clicked();

    void on_pushButton_50_clicked();

    void on_pushButton_51_clicked();

    void on_pushButton_57_clicked();

    void on_pushButton_53_clicked();

    void on_pushButton_55_clicked();

    void on_pushButton_58_clicked();

    void on_pushButton_56_clicked();

    void on_pushButton_clicked();

signals:

};

#endif // ADDRESSBOOKDIALOG_H
