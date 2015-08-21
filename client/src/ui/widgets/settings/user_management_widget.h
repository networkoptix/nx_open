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

    virtual void updateFromSettings() override;
private:
    QScopedPointer<Ui::QnUserManagementWidget> ui;
    QScopedPointer<QnUserManagementWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnUserManagementWidget)

};
