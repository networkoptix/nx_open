// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include "integrations_fwd.h"

class QObject;

namespace nx::vms::client::desktop::integrations {

/**
 * Create and register all integration instances in the special storage, which will own them.
 * Storage itself will be created as a singleton with the provided parent.
 */
void initialize(QObject* storageParent);

void connectionEstablished(
    const QnUserResourcePtr& currentUser,
    ec2::AbstractECConnectionPtr connection);

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

} // namespace nx::vms::client::desktop::integrations
