#ifndef QN_DATABASE_MANAGEMENT_WIDGET_H
#define QN_DATABASE_MANAGEMENT_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <utils/common/request_param.h> /* For QnHTTPRawResponse */
#include <utils/common/connective.h>

#include <ui/workbench/workbench_context_aware.h>

struct QnHTTPRawResponse;

namespace Ui {
    class DatabaseManagementWidget;
}

class QnDatabaseManagementWidget: public Connective<QWidget>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QWidget> base_type;

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
