#ifndef QN_DATABASE_MANAGEMENT_WIDGET_H
#define QN_DATABASE_MANAGEMENT_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtGui/QWidget>

namespace Ui {
    class DatabaseManagementWidget;
}

class QnDatabaseManagementWidget: public QWidget {
    Q_OBJECT

    typedef QWidget base_type;

public:
    QnDatabaseManagementWidget(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnDatabaseManagementWidget();

private slots:
    void at_backupButton_clicked();
    void at_restoreButton_clicked();

private:
    QScopedPointer<Ui::DatabaseManagementWidget> ui;
};


#endif // QN_DATABASE_MANAGEMENT_WIDGET_H
