#ifndef QN_DATABASE_MANAGEMENT_WIDGET_H
#define QN_DATABASE_MANAGEMENT_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

struct QnHTTPRawResponse;

namespace Ui {
    class DatabaseManagementWidget;
}

class QnDatabaseManagementWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;

public:
    QnDatabaseManagementWidget(QWidget *parent = NULL);
    virtual ~QnDatabaseManagementWidget();

private slots:
    void at_backupButton_clicked();
    void at_restoreButton_clicked();

private:
    QScopedPointer<Ui::DatabaseManagementWidget> ui;
};


#endif // QN_DATABASE_MANAGEMENT_WIDGET_H
