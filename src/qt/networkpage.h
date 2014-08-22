#ifndef NETWORKPAGE_H
#define NETWORKPAGE_H

#include <QDialog>

namespace Ui {
    class NetworkPage;
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
class NetworkPage : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkPage(QWidget *parent = 0);
    ~NetworkPage();

public slots:

private:
    Ui::NetworkPage *ui;
    QString returnValue;
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;

private slots:

signals:

};

#endif // ADDRESSBOOKDIALOG_H
