#include <gtest/gtest.h>

#include <nx/utils/log/log.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

// #TODO: #akulikov move below to the test class
TEST(SaveCamera_, invalidData)
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

    NX_INFO(this, "Both id and physicalId fields correctly defined.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData);

    NX_INFO(this, "Error case: id doesn't match physicalId.");
    cameraData.physicalId = "non-matching physicalId";
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", "physicalId", "parentId", "typeId", "vendor"}),
        nx::network::http::StatusCode::badRequest);

    NX_INFO(this, "Create another camera with auto-generated id.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ "physicalId", "parentId", "typeId", "vendor"}));

    NX_INFO(this, "Merge by id with the existing object.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({"id", /*"physicalId",*/ "parentId", "typeId", "vendor"}));

    NX_INFO(this, "Error case: both id and physicalId are missing.");
    NX_TEST_API_POST(&launcher, "/ec2/saveCamera", cameraData,
        keepOnlyJsonFields({/*"id",*/ /*"physicalId",*/ "parentId", "typeId", "vendor"}),
        nx::network::http::StatusCode::forbidden);
}

class SaveCamera: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_server.addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        ASSERT_TRUE(m_server.start());
        m_cameraData.parentId = QnUuid::createUuid();
        m_cameraData.typeId = qnResTypePool->getResourceTypeByName("Camera")->getId();
        m_cameraData.vendor = "test vendor";
        m_cameraData.physicalId = "matching physicalId";
        m_cameraData.id = nx::vms::api::CameraData::physicalIdToId(m_cameraData.physicalId);
    }

    void whenCameraAdded()
    {
        NX_TEST_API_POST(&m_server, "/ec2/saveCamera", m_cameraData);
    }

    void thenItShouldAppearInFullInfo()
    {
        vms::api::FullInfoData fullInfoData;
        NX_TEST_API_GET(&m_server, "/ec2/getFullInfo", &fullInfoData);
        ASSERT_TRUE(std::any_of(fullInfoData.cameras.cbegin(), fullInfoData.cameras.cend(),
            [this](const auto& cameraData) { return cameraData.id == m_cameraData.id; }));
    }

    void thenItShouldAppearInGetCameras()
    {
        vms::api::CameraDataList cameraDataList;
        NX_TEST_API_GET(&m_server, "/ec2/getCameras", &cameraDataList);
        ASSERT_TRUE(std::any_of(cameraDataList.cbegin(), cameraDataList.cend(),
            [this](const auto& cameraData) { return cameraData.id == m_cameraData.id; }));
    }

private:
    MediaServerLauncher m_server;
    nx::vms::api::CameraData m_cameraData;
};

TEST_F(SaveCamera, CameraAppearsInFullInfo)
{
    whenCameraAdded();
    thenItShouldAppearInGetCameras();
    thenItShouldAppearInFullInfo();
}

} // namespace test
} // namespace nx
