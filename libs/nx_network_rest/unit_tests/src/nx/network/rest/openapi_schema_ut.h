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

/**%apidoc
 * %param[unused] *.id
 */
struct TestDataMap: std::map<nx::Uuid, TestData>
{
    using base_type = std::map<nx::Uuid, TestData>;
    using base_type::base_type;
    TestDataMap(base_type&& origin): base_type(std::move(origin)) {}
    const TestData& front() const { return begin()->second; }
    nx::Uuid getId() const { return (size() == 1) ? begin()->first : nx::Uuid(); }
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

struct Base64Model
{
    QByteArray param; /**<%apidoc:base64 */

    QByteArray getId() const { return param; }
};

} // namespace nx::network::rest::json::test
