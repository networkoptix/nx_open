#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QCheckBox>

#include <business/business_fwd.h>
#include <health/system_health.h>

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

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

private:
    QList<QnBusiness::EventType> watchedEvents() const;
    quint64 watchedSystemHealth() const;

private:
    QScopedPointer<Ui::PopupSettingsWidget> ui;
    QMap<QnBusiness::EventType, QCheckBox* > m_businessRulesCheckBoxes;
    QMap<QnSystemHealth::MessageType, QCheckBox* > m_systemHealthCheckBoxes;
    QnBusinessEventsFilterResourcePropertyAdaptor *m_adaptor;
    bool m_updating;
};
