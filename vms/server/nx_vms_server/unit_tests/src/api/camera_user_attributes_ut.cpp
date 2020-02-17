#include <gtest/gtest.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

// NOTE: The hard-coded cameraId should match the hard-coded physicalId.
static const QByteArray kCameraAttributesData = /*suppress newline*/1 + R"json(
{
    "audioEnabled": false,
    "backupType": "CameraBackupDefault",
    "cameraId": "{58635275-79d5-b7a4-e3fc-115b622c298f}",
    "cameraName": "",
    "controlEnabled": true,
    "dewarpingParams": "",
    "disableDualStreaming": false,
    "failoverPriority": "Medium",
    "licenseUsed": true,
    "logicalId": "1",
    "maxArchiveDays": -30,
    "minArchiveDays": -1,
    "motionMask": "",
    "motionType": "0",
    "preferredServerId": "{00000000-0000-0000-0000-000000000000}",
    "recordAfterMotionSec": 5,
    "recordBeforeMotionSec": 5,
    "scheduleEnabled": false,
    "scheduleTasks":
    [
        {
            "bitrateKbps": 10500,
            "dayOfWeek": 6,
            "endTime": 78,
            "fps": 117,
            "recordingType": "RT_MotionOnly",
            "startTime": 42,
            "streamQuality": "normal"
        }
    ],
    "userDefinedGroupName": ""
}
)json";

class CameraUserAttributes: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_server.addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        ASSERT_TRUE(m_server.start());

        bool success;
        m_cameraAttributesData = QJson::deserialized(
            kCameraAttributesData, nx::vms::api::CameraAttributesData{}, &success);
        ASSERT_TRUE(success);

        m_cameraAttributesDataJson = QJson::serialized(m_cameraAttributesData);
        ASSERT_FALSE(m_cameraAttributesDataJson.isEmpty());

        // This check is important - it makes sure that the JSON was deserialized properly.
        ASSERT_EQ(
            QString::fromLatin1(kCameraAttributesData).simplified().replace(" ", "").toStdString(),
            m_cameraAttributesDataJson.simplified().replace(" ", "").toStdString());

        m_cameraData.parentId = QnUuid::createUuid();
        m_cameraData.typeId = qnResTypePool->getResourceTypeByName("Camera")->getId();
        m_cameraData.vendor = "test vendor";
        m_cameraData.physicalId = "matching physicalId";
        m_cameraData.id = m_cameraAttributesData.cameraId;

        // Check that cameraId in the hard-coded Json matches the hard-coded physicalId.
        ASSERT_EQ(
            nx::vms::api::CameraData::physicalIdToId(m_cameraData.physicalId).toStdString(),
            m_cameraAttributesData.cameraId.toStdString());
    }

    void saveCamera() const
    {
        NX_TEST_API_POST(&m_server, "/ec2/saveCamera", m_cameraData);
    }

    void saveAndGetCameraUserAttributes() const
    {
        NX_TEST_API_POST(&m_server, "/ec2/saveCameraUserAttributes", m_cameraAttributesData);

        nx::vms::api::CameraAttributesDataList cameraAttributesDataList;
        NX_TEST_API_GET(&m_server, "/ec2/getCameraUserAttributesList", &cameraAttributesDataList);

        ASSERT_EQ(1, cameraAttributesDataList.size());

        const QByteArray receivedCameraAttributesDataJson =
            QJson::serialized(cameraAttributesDataList.at(0));

        ASSERT_STREQ(m_cameraAttributesDataJson, receivedCameraAttributesDataJson);
    }

private:
    MediaServerLauncher m_server;
    nx::vms::api::CameraData m_cameraData;
    nx::vms::api::CameraAttributesData m_cameraAttributesData;
    QByteArray m_cameraAttributesDataJson;
};

TEST_F(CameraUserAttributes, ValidValues)
{
    saveCamera();
    saveAndGetCameraUserAttributes();
}

} // namespace test
} // namespace nx
