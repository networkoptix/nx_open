#ifndef POPUP_SETTINGS_WIDGET_H
#define POPUP_SETTINGS_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QCheckBox>

#include <business/business_fwd.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class PopupSettingsWidget;
}

class QnBusinessEventsFilterResourcePropertyAdaptor;

class QnPopupSettingsWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnPopupSettingsWidget(QWidget *parent = 0);
    ~QnPopupSettingsWidget();

    virtual void submitToSettings() override;
    virtual void updateFromSettings() override;

private slots:
    void at_showAllCheckBox_toggled(bool checked);
    void at_showBusinessEvents_valueChanged();

private:
    QScopedPointer<Ui::PopupSettingsWidget> ui;
    QMap<QnBusiness::EventType, QCheckBox* > m_businessRulesCheckBoxes;
    QList<QCheckBox* > m_systemHealthCheckBoxes;
    QnBusinessEventsFilterResourcePropertyAdaptor *m_adaptor;
};

#endif // POPUP_SETTINGS_WIDGET_H
