#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

namespace Ui {
    class QnUserManagementWidget;
}

class QnUserManagementWidgetPrivate;
class QnUserManagementWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    explicit QnUserManagementWidget(QWidget *parent = 0);
    ~QnUserManagementWidget();

private:
    QScopedPointer<Ui::QnUserManagementWidget> ui;
    QnUserManagementWidgetPrivate *d;
    friend class QnUserManagementWidgetPrivate;

private slots:
    void at_refreshButton_clicked();
    void at_mergeLdapUsersAsync_finished(int status, int handle, const QString &errorString);
};
