// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/vms/api/data/device_model.h>

namespace nx::network::rest::json::test {

struct NestedTestData
{
    nx::Uuid id;
    QString name;
};

struct TestData: NestedTestData
{
    std::vector<NestedTestData> list;
};

struct TestDataMap: std::map<nx::Uuid, TestData>
{
    // TODO: Implement & use ValueOrMap instead.
    [[nodiscard]] const auto& getId() const
    {
        static const key_type defaultValue{};
        const key_type& result = 1 == size() ? begin()->first : defaultValue;

        // decltype(auto) will be deduced as const reference here
        return result;
    }
};

/**%apidoc Device information object.
 * %param[unused] attributes.cameraId
 * %param[opt]:object parameters Device specific key-value parameters.
 */
struct TestDeviceModel
{
    nx::vms::api::CameraData general;
    std::optional<nx::vms::api::Credentials> credentials;
    std::optional<nx::vms::api::CameraAttributesData> attributes;
    std::optional<nx::vms::api::ResourceStatus> status;
};
NX_REFLECTION_INSTRUMENT(TestDeviceModel, (general)(credentials)(attributes)(status))

struct Base64Model
{
    QByteArray param; /**<%apidoc:base64 */

    QByteArray getId() const { return param; }
};

struct ArrayOrderedTestNested
{
    std::string name;
};
NX_REFLECTION_INSTRUMENT(ArrayOrderedTestNested, (name))

struct ArrayOrderedTestVariant
{
    std::variant<std::string, int, ArrayOrderedTestNested> id;
};
NX_REFLECTION_INSTRUMENT(ArrayOrderedTestVariant, (id))

struct ArrayOrdererTestResponse
{
    std::map<std::string, std::vector<ArrayOrderedTestVariant>> map;
};
NX_REFLECTION_INSTRUMENT(ArrayOrdererTestResponse, (map))

} // namespace nx::network::rest::json::test
