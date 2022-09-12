// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QWidget>

#include <health/system_health.h>
#include <nx/vms/event/event_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class PopupSettingsWidget;
}

namespace nx::vms::common { class BusinessEventFilterResourcePropertyAdaptor; }
namespace nx::vms::event { class StringsHelper; }

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
    QList<nx::vms::api::EventType> watchedEvents() const;
    QSet<QnSystemHealth::MessageType> watchedSystemHealth() const;

private:
    QScopedPointer<Ui::PopupSettingsWidget> ui;
    QMap<nx::vms::api::EventType, QCheckBox*> m_businessRulesCheckBoxes;
    QMap<QnSystemHealth::MessageType, QCheckBox*> m_systemHealthCheckBoxes;
    nx::vms::common::BusinessEventFilterResourcePropertyAdaptor* m_adaptor;
    bool m_updating;
    std::unique_ptr<nx::vms::event::StringsHelper> m_helper;
};
