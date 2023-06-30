// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/widgets/abstract_search_widget.h>

namespace nx::vms::client::core { class AnalyticsSearchSetup; }

namespace nx::vms::client::desktop {


class AnalyticsSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    AnalyticsSearchWidget(WindowContext* context, QWidget* parent = nullptr);
    virtual ~AnalyticsSearchWidget() override;

    virtual void resetFilters() override;

    core::AnalyticsSearchSetup* analyticsSetup() const;

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
    virtual bool calculateAllowance() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
