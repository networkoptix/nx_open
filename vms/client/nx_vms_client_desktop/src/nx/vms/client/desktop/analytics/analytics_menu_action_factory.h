#pragma once

#include <nx/vms/client/desktop/ui/actions/action_factories.h>

namespace nx::vms::client::desktop {

class AnalyticsMenuActionFactory: public ui::action::Factory
{
    Q_OBJECT
    using base_type = ui::action::Factory;

public:
    explicit AnalyticsMenuActionFactory(QObject* parent = nullptr);
    virtual QList<QAction*> newActions(const ui::action::Parameters& parameters,
        QObject* parent) override;
};

} // namespace nx::vms::client::desktop
