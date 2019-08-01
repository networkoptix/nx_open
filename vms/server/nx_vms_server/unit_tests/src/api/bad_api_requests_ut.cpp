#include <memory>
#include <gtest/gtest.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <test_support/mediaserver_launcher.h>

namespace nx {
namespace test {

class BadApiRequests: public ::testing::Test
{
private:
    std::unique_ptr<MediaServerLauncher> m_server;
    nx::vms::api::CameraDataList m_cameras;

public:

    virtual void SetUp() override
    {
        m_server = std::unique_ptr<MediaServerLauncher>(
            new MediaServerLauncher(/* tmpDir */ ""));
        m_server->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
        ASSERT_TRUE(m_server->start());

        m_cameras.clear();
        for (int i = 0; i < 10; ++i)
        {
            nx::vms::api::CameraData cameraData;
            cameraData.parentId = QnUuid::createUuid();
            cameraData.typeId = qnResTypePool->getResourceTypeByName("Camera")->getId();
            cameraData.vendor = "test vendor";
            cameraData.physicalId = lit("matching physicalId %1").arg(i);
            cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);
            m_cameras.push_back(cameraData);
        }

        auto connection = m_server->commonModule()->ec2Connection();
        auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        cameraManager->addCamerasSync(m_cameras);
    }

    void removeCamerasOneByOne()
    {
        auto connection = m_server->commonModule()->ec2Connection();
        auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);

        QnMutex mutex;
        QnWaitCondition waitCondition;
        int runningRequests = 0;
        int finishedRequests = 0;

        QnMutexLocker lock(&mutex);

        for (int i = 0; i < m_cameras.size(); ++i)
        {
            int reqId = resourceManager->remove(m_cameras[i].id,
            [&](ec2::ErrorCode errorCode)
            {
                QnMutexLocker lock(&mutex);
                ++finishedRequests;
                waitCondition.wakeAll();
                ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
            });
            ++runningRequests;
            if (i % 3)
            {
                ec2::impl::SimpleHandlerPtr handler;
                int reqId = resourceManager->remove(m_cameras[i].id,
                    [&](ec2::ErrorCode errorCode)
                    {
                        QnMutexLocker lock(&mutex);
                        ++finishedRequests;
                        waitCondition.wakeAll();
                        ASSERT_EQ(ec2::ErrorCode::badRequest, errorCode);
                    });
                ++runningRequests;
            }
        }
        while (finishedRequests < runningRequests)
            waitCondition.wait(&mutex);
        auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        nx::vms::api::CameraDataList cameraList;
        ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->getCamerasSync(&cameraList));
        ASSERT_EQ(0, cameraList.size());
    }

    void removeSeveralCamerasAtOnce()
    {
        auto connection = m_server->commonModule()->ec2Connection();
        auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);

        QnMutex mutex;
        QnWaitCondition waitCondition;
        int finishedRequests = 0;

        QnMutexLocker lock(&mutex);

        QVector<QnUuid> idList;
        for (int i = 0; i < m_cameras.size(); ++i)
            idList << m_cameras[i].id;

        QVector<QnUuid> badIdList = idList;
        badIdList.push_back(QnUuid::createUuid());

        resourceManager->remove(badIdList,
            [&](ec2::ErrorCode errorCode)
        {
            QnMutexLocker lock(&mutex);
            ++finishedRequests;
            waitCondition.wakeAll();
            ASSERT_EQ(ec2::ErrorCode::badRequest, errorCode);
        });

        resourceManager->remove(idList,
            [&](ec2::ErrorCode errorCode)
        {
            QnMutexLocker lock(&mutex);
            ++finishedRequests;
            waitCondition.wakeAll();
            ASSERT_EQ(ec2::ErrorCode::ok, errorCode);
        });

        while (finishedRequests < 2)
            waitCondition.wait(&mutex);

        auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        nx::vms::api::CameraDataList cameraList;
        ASSERT_EQ(ec2::ErrorCode::ok, cameraManager->getCamerasSync(&cameraList));
        ASSERT_EQ(0, cameraList.size());
    }

};

TEST_F(BadApiRequests, removeCamerasOneByOne)
{
    removeCamerasOneByOne();
}

TEST_F(BadApiRequests, removeSeveralCamerasAtOnce)
{
    removeCamerasOneByOne();
}

} // namespace test
} // namespace nx
