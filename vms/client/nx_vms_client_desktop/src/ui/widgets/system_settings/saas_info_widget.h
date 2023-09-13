// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace nx::vms::client::desktop {

class SaasInfoWidget:
    public QnAbstractPreferencesWidget,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    explicit SaasInfoWidget(SystemContext* systemContext, QWidget* parent = nullptr);
    virtual ~SaasInfoWidget() override;

    virtual void loadDataToUi() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
