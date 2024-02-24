// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/event/event_parameters.h>

namespace nx::vms::client::desktop {
namespace ui {

class AnalyticsSdkEventModel:
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

    AnalyticsSdkEventModel(SystemContext* systemContext, QObject* parent = nullptr);
    ~AnalyticsSdkEventModel();

    void loadFromCameras(
        const QnVirtualCameraResourceList& cameras,
        const nx::vms::event::EventParameters& currentEventParameters);

    void loadFromCameras(
        const QnVirtualCameraResourceList& cameras,
        nx::Uuid engineId,
        QString eventTypeId);

    bool isValid() const;
};

} // namespace ui
} // namespace nx::vms::client::desktop
