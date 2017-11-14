#pragma once

#include <nx/client/desktop/ui/actions/action_text_factories.h>
#include <nx/client/desktop/ui/actions/action_conditions.h>

namespace nx {
namespace client {
namespace desktop {
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
} // namespace desktop
} // namespace client
} // namespace nx
