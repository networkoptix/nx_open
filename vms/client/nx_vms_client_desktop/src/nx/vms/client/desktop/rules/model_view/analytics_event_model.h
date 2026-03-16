// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/event/event_parameters.h>

namespace nx::vms::client::desktop::rules {

/** Tree model of all Analytics event types available for selected devices.*/
class AnalyticsEventModel:
    public QStandardItemModel,
    public SystemContextAware
{
public:
    enum DataRole
    {
        EventTypeIdRole = Qt::UserRole + 1,
        EngineIdRole,
        ValidEventRole,
    };

    AnalyticsEventModel(SystemContext* systemContext, QObject* parent = nullptr);
    ~AnalyticsEventModel();

    void loadFromCameras(
        const QnVirtualCameraResourceList& cameras,
        nx::Uuid engineId,
        QString eventTypeId);

    bool isValid() const;
};

} // namespace nx::vms::client::desktop::rules
