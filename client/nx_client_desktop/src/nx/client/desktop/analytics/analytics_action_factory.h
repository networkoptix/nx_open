#pragma once

#include <nx/client/desktop/ui/actions/action_factories.h>

namespace nx {
namespace client {
namespace desktop {

class AnalyticsActionFactory: public ui::action::Factory
{
    Q_OBJECT
    using base_type = ui::action::Factory;

public:
    explicit AnalyticsActionFactory(QObject* parent);
    virtual QList<QAction*> newActions(const ui::action::Parameters& parameters,
        QObject* parent) override;
};

} // namespace desktop
} // namespace client
} // namespace nx
