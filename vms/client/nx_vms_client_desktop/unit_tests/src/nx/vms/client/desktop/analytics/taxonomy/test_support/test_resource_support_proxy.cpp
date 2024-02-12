// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_resource_support_proxy.h"

namespace nx::vms::client::desktop::analytics::taxonomy {

TestResourceSupportProxy::TestResourceSupportProxy(
    std::map<QString, TypeSupportInfo> typeSupportInfo)
    :
    m_typeSupportInfo(std::move(typeSupportInfo))
{
}

bool TestResourceSupportProxy::isEntityTypeSupported(
    nx::analytics::taxonomy::EntityType entityType,
    const QString& entityTypeId,
    nx::Uuid deviceId,
    nx::Uuid engineId) const
{
    if (entityType != nx::analytics::taxonomy::EntityType::objectType)
        return false;

    if (!m_typeSupportInfo.contains(entityTypeId))
        return false;

    const auto& attributeSupportInfo = m_typeSupportInfo.at(entityTypeId);
    if (!engineId.isNull())
    {
        for (const auto& [attributeName, supportInfo]: attributeSupportInfo)
        {
            if (supportInfo.contains(engineId))
            {
                const auto& deviceSupportInfo = supportInfo.at(engineId);

                if (deviceId.isNull())
                    return true;

                if (deviceSupportInfo.contains(deviceId))
                    return true;
            }
        }
    }
    else
    {
        for (const auto& [attributeName, supportInfo]: attributeSupportInfo)
        {

            for (const auto& [engineId, deviceIds]: supportInfo)
            {
                if (deviceId.isNull())
                    return true;

                if (deviceIds.contains(deviceId))
                    return true;
            }
        }
    }

    return false;
}

bool TestResourceSupportProxy::isEntityTypeAttributeSupported(
    nx::analytics::taxonomy::EntityType entityType,
    const QString& entityTypeId,
    const QString& fullAttributeName,
    nx::Uuid deviceId,
    nx::Uuid engineId) const
{
    if (entityType != nx::analytics::taxonomy::EntityType::objectType)
        return false;

    if (!m_typeSupportInfo.contains(entityTypeId))
        return false;

    const auto& attributeSupportInfo = m_typeSupportInfo.at(entityTypeId);

    if (!attributeSupportInfo.contains(fullAttributeName))
        return false;

    const auto& supportInfo = attributeSupportInfo.at(fullAttributeName);

    if (!engineId.isNull())
    {
        if (!supportInfo.contains(engineId))
            return false;

        if (deviceId.isNull())
            return true;

        return supportInfo.at(engineId).contains(deviceId);
    }
    else
    {
        if (deviceId.isNull())
            return true;

        for (const auto& [engineId, deviceIds]: supportInfo)
        {
            if (deviceIds.contains(deviceId))
                return true;
        }

        return false;
    }
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
