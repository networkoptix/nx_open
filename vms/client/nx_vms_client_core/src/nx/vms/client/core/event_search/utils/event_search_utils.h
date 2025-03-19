// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>
#include <chrono>

#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/event_search/models/fetch_request.h>
#include <nx/vms/client/core/resource/resource_fwd.h>

namespace nx::vms::client::core {

class SystemContext;

class NX_VMS_CLIENT_CORE_API EventSearchUtils: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    EventSearchUtils(QObject* parent = nullptr);

    Q_INVOKABLE static QString timeSelectionText(EventSearch::TimeSelection value);

    static QString cameraSelectionText(
        EventSearch::CameraSelection selection,
        const QnVirtualCameraResourceSet& cameras);

    Q_INVOKABLE QString cameraSelectionText(
        EventSearch::CameraSelection selection,
        const UuidList& cameraIds,
        nx::vms::client::core::SystemContext* context);

    Q_INVOKABLE static FetchRequest fetchRequest(
        EventSearch::FetchDirection direction,
        qint64 centralPointUs);

    Q_INVOKABLE QString timestampText(
        qint64 timestampMs,
        nx::vms::client::core::SystemContext* context) const;

    static QString timestampText(
        const std::chrono::milliseconds& timestampMs,
        SystemContext* context);

    static QString timeFromNowText(
        const std::chrono::microseconds& timestampMs,
        SystemContext* context);
};

} // namespace nx::vms::client::core
