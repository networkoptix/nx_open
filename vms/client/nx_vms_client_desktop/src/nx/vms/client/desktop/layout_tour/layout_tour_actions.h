// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/ui/actions/action_text_factories.h>
#include <nx/vms/client/desktop/ui/actions/action_conditions.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

class LayoutTourTextFactory: public TextFactory
{
    Q_OBJECT
    using base_type = TextFactory;

public:
    explicit LayoutTourTextFactory(QObject* parent = nullptr);

    virtual QString text(const Parameters& parameters,
        QnWorkbenchContext* context) const override;
};

namespace condition
{

/** Layout tour is running. */
ConditionWrapper tourIsRunning();

/** Layout tour is running. */
ConditionWrapper canStartTour();

} // namespace condition

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
