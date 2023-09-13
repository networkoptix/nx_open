// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class SystemContext;

class AdvancedSystemSettingsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    enum class Subpage
    {
        backup,
        logs,
    };

    static QUrl urlFor(Subpage page);

    explicit AdvancedSystemSettingsWidget(SystemContext* context, QWidget *parent = nullptr);
    ~AdvancedSystemSettingsWidget();

    virtual bool isNetworkRequestRunning() const override;
    virtual bool activate(const QUrl& url) override;
    virtual void discardChanges() override;

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
