#ifndef QN_GENERAL_SYSTEM_ADMINISTRATION_WIDGET_H
#define QN_GENERAL_SYSTEM_ADMINISTRATION_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui_general_system_administration_widget.h>

struct QnHTTPRawResponse;

class QnGeneralSystemAdministrationWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;
public:
    QnGeneralSystemAdministrationWidget(QWidget *parent = NULL);

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

    virtual bool hasChanges() const override;
private:
    QScopedPointer<Ui::GeneralSystemAdministrationWidget> ui;
};


#endif // QN_GENERAL_SYSTEM_ADMINISTRATION_WIDGET_H
