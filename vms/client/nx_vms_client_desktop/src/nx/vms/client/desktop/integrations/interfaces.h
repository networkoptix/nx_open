#pragma once

#include <QtCore/QObject>

#include "integrations_fwd.h"

namespace nx::vms::client::desktop {

/**
 * Base class for all client integrations.
 */
class Integration: public QObject
{
    Q_OBJECT

public:
    explicit Integration(QObject* parent = nullptr);
};

namespace integrations {

/**
 * Integration interface, allowing to add custom context menu actions.
 */
struct IActionFactory
{
    virtual ~IActionFactory() = default;
    virtual void registerActions(ui::action::MenuFactory* factory) = 0;
};


} // namespace integrations
} // namespace nx::vms::client::desktop
