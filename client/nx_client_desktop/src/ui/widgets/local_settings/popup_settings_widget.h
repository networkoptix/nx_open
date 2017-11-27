#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QCheckBox>

#include <nx/vms/event/event_fwd.h>
#include <health/system_health.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class PopupSettingsWidget;
}

namespace nx { namespace vms { namespace event { class StringsHelper; }}}

class QnBusinessEventsFilterResourcePropertyAdaptor;

class QnPopupSettingsWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit QnPopupSettingsWidget(QWidget *parent = 0);
    ~QnPopupSettingsWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

private:
    QList<nx::vms::event::EventType> watchedEvents() const;
    QSet<QnSystemHealth::MessageType> watchedSystemHealth() const;

private:
    QScopedPointer<Ui::PopupSettingsWidget> ui;
    QMap<nx::vms::event::EventType, QCheckBox*> m_businessRulesCheckBoxes;
    QMap<QnSystemHealth::MessageType, QCheckBox*> m_systemHealthCheckBoxes;
    QnBusinessEventsFilterResourcePropertyAdaptor* m_adaptor;
    bool m_updating;
    std::unique_ptr<nx::vms::event::StringsHelper> m_helper;
};
