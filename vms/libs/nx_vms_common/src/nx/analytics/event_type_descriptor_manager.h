// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::analytics {

namespace taxonomy { class DescriptorContainer; }

class NX_VMS_COMMON_API EventTypeDescriptorManager: public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit EventTypeDescriptorManager(
        taxonomy::DescriptorContainer* taxonomyDescriptorContainer,
        QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::EventTypeDescriptor> descriptor(
        const nx::vms::api::analytics::EventTypeId& id) const;

    nx::vms::api::analytics::EventTypeDescriptorMap descriptors(
        const std::set<nx::vms::api::analytics::EventTypeId>& eventTypeIds = {}) const;

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes only those Event types, which are actually available, so only compatible, enabled
     * and running Engines are used.
     */
    nx::vms::api::analytics::ScopedEventTypeIds supportedEventTypeIds(
        const QnVirtualCameraResourcePtr& device) const;

    nx::vms::api::analytics::ScopedEventTypeIds supportedEventTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    nx::vms::api::analytics::ScopedEventTypeIds supportedEventTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

    nx::vms::api::analytics::EventTypeDescriptorMap supportedEventTypeDescriptors(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * Tree of the Event type ids. Root nodes are Engines, then Groups and Event types as leaves.
     * Includes all Event types, which can theoretically be available on this Device, so all
     * compatible Engines are used.
     */
    nx::vms::api::analytics::ScopedEventTypeIds compatibleEventTypeIds(
        const QnVirtualCameraResourcePtr& device) const;

    nx::vms::api::analytics::ScopedEventTypeIds compatibleEventTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    nx::vms::api::analytics::ScopedEventTypeIds compatibleEventTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

private:
    nx::vms::api::analytics::ScopedEventTypeIds eventTypeGroupsByEngines(
        const std::set<nx::vms::api::analytics::EngineId>& engineIds,
        const std::set<nx::vms::api::analytics::EventTypeId>& eventTypeIds) const;

private:
    taxonomy::DescriptorContainer* const m_taxonomyDescriptorContainer;
};

} // namespace nx::analytics
