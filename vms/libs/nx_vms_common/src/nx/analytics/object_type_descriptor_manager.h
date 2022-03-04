// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::analytics {

namespace taxonomy { class DescriptorContainer; }

class NX_VMS_COMMON_API ObjectTypeDescriptorManager:
    public QObject
{
    using base_type = QObject;
    Q_OBJECT

public:
    explicit ObjectTypeDescriptorManager(
        taxonomy::DescriptorContainer* taxonomyDescriptorContainer,
        QObject* parent = nullptr);

    std::optional<nx::vms::api::analytics::ObjectTypeDescriptor> descriptor(
        const nx::vms::api::analytics::ObjectTypeId& id) const;

    nx::vms::api::analytics::ObjectTypeDescriptorMap descriptors(
        const std::set<nx::vms::api::analytics::ObjectTypeId>& objectTypeIds = {}) const;

    nx::vms::api::analytics::ScopedObjectTypeIds supportedObjectTypeIds(
        const QnVirtualCameraResourcePtr& device) const;

    nx::vms::api::analytics::ScopedObjectTypeIds supportedObjectTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    nx::vms::api::analytics::ScopedObjectTypeIds supportedObjectTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

    nx::vms::api::analytics::ScopedObjectTypeIds compatibleObjectTypeIds(
        const QnVirtualCameraResourcePtr& device) const;

    nx::vms::api::analytics::ScopedObjectTypeIds compatibleObjectTypeIdsUnion(
        const QnVirtualCameraResourceList& devices) const;

    nx::vms::api::analytics::ScopedObjectTypeIds compatibleObjectTypeIdsIntersection(
        const QnVirtualCameraResourceList& devices) const;

private:
    nx::vms::api::analytics::GroupId objectTypeGroupForEngine(
        const nx::vms::api::analytics::EngineId& engineId,
        const nx::vms::api::analytics::ObjectTypeId& objectTypeId) const;

private:
    taxonomy::DescriptorContainer* const m_taxonomyDescriptorContainer;
};

} // namespace nx::analytics
