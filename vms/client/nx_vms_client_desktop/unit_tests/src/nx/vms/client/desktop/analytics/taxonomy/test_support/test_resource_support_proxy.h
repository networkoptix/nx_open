// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>

#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

class TestResourceSupportProxy: public nx::analytics::taxonomy::AbstractResourceSupportProxy
{
public:
    using TypeSupportInfo = std::map<
        QString /*attributeName*/,
        std::map<QnUuid /*engineId*/, std::set<QnUuid /*deviceId*/>>
    >;

public:
    TestResourceSupportProxy(std::map<QString, TypeSupportInfo> supportInfo);

    virtual bool isEntityTypeSupported(
        nx::analytics::taxonomy::EntityType entityType,
        const QString& entityTypeId,
        QnUuid deviceId,
        QnUuid engineId) const override;

    virtual bool isEntityTypeAttributeSupported(
        nx::analytics::taxonomy::EntityType entityType,
        const QString& entityTypeId,
        const QString& fullAttributeName,
        QnUuid deviceId,
        QnUuid engineId) const override;

private:
    std::map<QString, TypeSupportInfo> m_typeSupportInfo;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
