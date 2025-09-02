// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>

#include "integrations_fwd.h"

namespace nx::vms::client::desktop::integrations {

/**
 * Create and register all integration instances in the special storage, which will own them.
 */
class Storage: public QObject
{
public:
    explicit Storage(bool desktopMode);
    virtual ~Storage() override;

    void connectionEstablished(
        const QnUserResourcePtr& currentUser, ec2::AbstractECConnectionPtr connection);

    /**
     * Register custom menu actions, provided by the integrations.
     */
    void registerActions(menu::MenuFactory* factory);

    /**
     * Called after widget is added to the scene.
     */
    void registerWidget(QnMediaResourceWidget* widget);

    /**
     * Called before widget is destroyed.
     */
    void unregisterWidget(QnMediaResourceWidget* widget);

    /**
     * Paint overlays over video.
     */
    void paintVideoOverlays(QnMediaResourceWidget* widget, QPainter* painter);

private:
    struct Private;

    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::integrations
