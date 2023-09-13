// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui { class LogsManagementWidget; }

namespace nx::vms::client::desktop {

class LogsManagementWidget:
    public QnAbstractPreferencesWidget,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit LogsManagementWidget(SystemContext* context, QWidget* parent = nullptr);
    virtual ~LogsManagementWidget();

    virtual bool isNetworkRequestRunning() const override;
    virtual void discardChanges() override;

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

private:
    void setupUi();
    void updateWidgets(LogsManagementWatcher::State state);

private:
    QScopedPointer<Ui::LogsManagementWidget> ui;
    QPointer<LogsManagementWatcher> m_watcher;
    bool needUpdateBeforeClosing = false;
};

} // namespace nx::vms::client::desktop
