#ifndef QN_GENERAL_SYSTEM_ADMINISTRATION_WIDGET_H
#define QN_GENERAL_SYSTEM_ADMINISTRATION_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

struct QnHTTPRawResponse;

namespace Ui {
    class GeneralSystemAdministrationWidget;
}

class QnGeneralSystemAdministrationWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;
public:
    QnGeneralSystemAdministrationWidget(QWidget *parent = NULL);

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;
private:
    QScopedPointer<Ui::GeneralSystemAdministrationWidget> ui;
};


#endif // QN_GENERAL_SYSTEM_ADMINISTRATION_WIDGET_H
