// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QWidget>

#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/event_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class PopupSettingsWidget;
}

namespace nx::vms::event { class StringsHelper; }

namespace nx::vms::client::desktop {

class SiteNotificationSettingsManager;

class PopupSettingsWidget: public QnAbstractPreferencesWidget
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit PopupSettingsWidget(
        SiteNotificationSettingsManager* siteNotificationSettingsManager,
        QWidget* parent = nullptr);
    ~PopupSettingsWidget();

    virtual void applyChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

private:
    QList<nx::vms::api::EventType> watchedEvents() const;
    std::set<nx::vms::common::system_health::MessageType> storedSystemHealth() const;

private:
    QScopedPointer<Ui::PopupSettingsWidget> ui;
    QMap<nx::vms::api::EventType, QCheckBox*> m_businessRulesCheckBoxes;
    QMap<nx::vms::common::system_health::MessageType, QCheckBox*> m_systemHealthCheckBoxes;
    bool m_updating;
    SiteNotificationSettingsManager* const m_siteNotificationSettingsManager;
    std::unique_ptr<nx::vms::event::StringsHelper> m_helper;
};

} // namespace nx::vms::client::desktop
