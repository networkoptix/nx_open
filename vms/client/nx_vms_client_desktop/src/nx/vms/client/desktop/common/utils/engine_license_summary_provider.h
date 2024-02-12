// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class EngineLicenseSummaryProvider: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    explicit EngineLicenseSummaryProvider(SystemContext* context, QObject* parent = nullptr);

    /** Returns license summary for the given engine. */
    Q_INVOKABLE QVariant licenseSummary(nx::Uuid engineId) const;

    /**
     * Returns license summary for the given engine with the proposed activated engines on
     * the camera with the given id.
     */
    Q_INVOKABLE QVariant licenseSummary(
        nx::Uuid engineId, nx::Uuid cameraId, const QVariantList& proposedEngines) const;

    QSet<nx::Uuid> overusedEngines(
        nx::Uuid cameraId, const QSet<nx::Uuid>& proposedEngines) const;
};

} // namespace nx::vms::client::desktop
