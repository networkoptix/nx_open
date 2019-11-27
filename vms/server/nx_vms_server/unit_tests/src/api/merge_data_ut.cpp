#include <gtest/gtest.h>

#include <nx/utils/log/log.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

TEST(MergeData, CameraUserAttributes)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    nx::vms::api::CameraData cameraData;
    cameraData.parentId = QnUuid::createUuid();
    auto resTypePtr = qnResTypePool->getResourceTypeByName("Camera");
    ASSERT_FALSE(resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    cameraData.vendor = "test vendor";
    cameraData.physicalId = "matching physicalId";
    cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData);

    nx::vms::api::CameraAttributesData attributes;
    attributes.cameraId = cameraData.id;
    attributes.logicalId = "1";
    attributes.scheduleEnabled = false; //< to pass no license check
    NX_TEST_API_POST(&launcher, "/ec2/saveCameraUserAttributes", attributes);

    nx::vms::api::CameraAttributesDataList receivedAttributes;
    NX_TEST_API_GET(&launcher, "/ec2/getCameraUserAttributesList", &receivedAttributes);
    ASSERT_EQ(receivedAttributes.size(), 1);
    ASSERT_EQ(receivedAttributes[0].logicalId, attributes.logicalId);

    attributes.cameraName = "test name";
    NX_TEST_API_POST(
        &launcher,
        "/ec2/saveCameraUserAttributes",
        attributes,
        keepOnlyJsonFields({"cameraId", "cameraName"}));
    NX_TEST_API_GET(&launcher, "/ec2/getCameraUserAttributesList", &receivedAttributes);
    ASSERT_EQ(receivedAttributes.size(), 1);
    ASSERT_EQ(receivedAttributes[0].cameraName, attributes.cameraName);
    ASSERT_EQ(receivedAttributes[0].logicalId, attributes.logicalId);

    const QByteArray list = lit(R"json(
        [
            {
                "cameraId": "%1",
                "cameraName": "modified name",
                "scheduleEnabled": false
            }
        ]
        )json").arg(cameraData.id.toString()).toUtf8();
    NX_TEST_API_POST(&launcher, "/ec2/saveCameraUserAttributesList", list);
    NX_TEST_API_GET(&launcher, "/ec2/getCameraUserAttributesList", &receivedAttributes);
    ASSERT_EQ(receivedAttributes.size(), 1);
    ASSERT_EQ(receivedAttributes[0].cameraName, "modified name");
    ASSERT_TRUE(receivedAttributes[0].logicalId.isEmpty()); //< no merge for list
}

} // namespace test
} // namespace nx
