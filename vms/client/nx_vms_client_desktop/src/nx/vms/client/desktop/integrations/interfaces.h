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
 * Integration interface, allowing to work directly with the server api.
 */
struct IServerApiProcessor
{
    virtual ~IServerApiProcessor() = default;

    virtual void connectionEstablished(ec2::AbstractECConnectionPtr connection) = 0;
};

/**
 * Integration interface, allowing to add custom context menu actions.
 */
struct IMenuActionFactory
{
    virtual ~IMenuActionFactory() = default;

    /**
     * Called during actions factory initialization.
     */
    virtual void registerActions(ui::action::MenuFactory* factory) = 0;
};

/**
 * Integration interface, allowing to paint video overlay.
 */
struct IOverlayPainter
{
    virtual ~IOverlayPainter() = default;

    /**
     * Called after widget is added to the scene.
     */
    virtual void registerWidget(QnMediaResourceWidget* /*widget*/) {}

    /**
     * Called before widget is destroyed.
     */
    virtual void unregisterWidget(QnMediaResourceWidget* /*widget*/) {}

    /**
     * Called after actual video frame is painted.
     */
    virtual void paintVideoOverlay(QnMediaResourceWidget* widget, QPainter* painter) = 0;
};

} // namespace integrations
} // namespace nx::vms::client::desktop
