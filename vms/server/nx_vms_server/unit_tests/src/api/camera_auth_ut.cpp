#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/camera_data.h>
#include <rest/server/json_rest_result.h>
#include <server_query_processor.h>
#include <nx/network/http/http_types.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/network_resource.h>

#include "test_api_requests.h"

namespace nx::test {

class CameraAuthUt: public ::testing::Test
{
protected:
    template <typename Data>
    ec2::QnTransaction<Data> createTran(const QString& name, const QString& value)
    {
        if constexpr (std::is_same<Data, vms::api::ResourceParamWithRefData>::value)
        {
            return ec2::QnTransaction<Data>(
                ec2::ApiCommand::setResourceParam,
                QnUuid::createUuid(),
                Data(QnUuid::createUuid(), name, value));
        }
        else
        {
            return ec2::QnTransaction<Data>(
                ec2::ApiCommand::setResourceParam,
                QnUuid::createUuid(),
                Data(name, value));
        }
    }

    template <typename Data>
    ec2::QnTransaction<Data> whenTransactionProcessed(const ec2::QnTransaction<Data>& originalTran)
    {
        return ec2::detail::amendTranIfNeeded(originalTran);
    }

    template<typename Data>
    void thenItShouldBeAmended(
        const ec2::QnTransaction<Data>& originalTran,
        const ec2::QnTransaction<Data>& processedTran)
    {
        ASSERT_NE(originalTran.params, processedTran.params);
    }

    template<typename Data>
    void thenItShouldNOTBeAmended(
        const ec2::QnTransaction<Data>& originalTran,
        const ec2::QnTransaction<Data>& processedTran)
    {
        ASSERT_EQ(originalTran.params, processedTran.params);
    }

    template<typename Data>
    void checkAmendTransaction()
    {
        auto originalTran = createTran<Data>(
            ResourcePropertyKey::kCredentials,
            "testValue");
        auto amendedTran = whenTransactionProcessed(originalTran);
        thenItShouldBeAmended(originalTran, amendedTran);

        originalTran = createTran<Data>(
            ResourcePropertyKey::kDefaultCredentials,
            "testValue");
        amendedTran = whenTransactionProcessed(originalTran);
        thenItShouldBeAmended(originalTran, amendedTran);

        originalTran = createTran<Data>(
            "some_other_name",
            "testValue");
        amendedTran = whenTransactionProcessed(originalTran);
        thenItShouldNOTBeAmended(originalTran, amendedTran);
    }

    template<typename Data>
    void checkOutputDataAmending()
    {
        assertParamsRestored<Data>(
            /* param name */ ResourcePropertyKey::kCredentials,
            /* access */ Qn::kSystemAccess,
            /* should be equal */ true);

        assertParamsRestored<Data>(
            /* param name */ ResourcePropertyKey::kDefaultCredentials,
            /* access */ Qn::kSystemAccess,
            /* should be equal */ true);

        assertParamsRestored<Data>(
            /* param name */ ResourcePropertyKey::kCredentials,
            /* access */ Qn::kVideowallUserAccess,
            /* should be equal */ true);

        assertParamsRestored<Data>(
            /* param name */ ResourcePropertyKey::kDefaultCredentials,
            /* access */ Qn::kVideowallUserAccess,
            /* should be equal */ true);

        assertParamsRestored<Data>(
            /* param name */ ResourcePropertyKey::kCredentials,
            Qn::UserAccessData(QnUuid::createUuid(), Qn::UserAccessData::Access::Default),
            /* should be equal */ false);

        assertParamsRestored<Data>(
            /* param name */ ResourcePropertyKey::kDefaultCredentials,
            Qn::UserAccessData(QnUuid::createUuid(), Qn::UserAccessData::Access::Default),
            /* should be equal */ false);
    }

    template<typename Data>
    void assertParamsRestored(
        const QString& paramName,
        const Qn::UserAccessData& accessData,
        bool shouldBeEqual)
    {
        auto originalTran = createTran<Data>(paramName, "user:password");
        auto amendedTran = whenTransactionProcessed(originalTran);
        ec2::amendOutputDataIfNeeded(accessData, &amendedTran.params);

        if (shouldBeEqual)
        {
            ASSERT_EQ(originalTran, amendedTran);
        }
        else
        {
            ASSERT_NE(originalTran, amendedTran);
            ASSERT_EQ("user:******", amendedTran.params.value);
        }
    }

};

class CameraAuthFt: public ::testing::Test
{
protected:
    CameraAuthFt()
    {
        m_server = std::unique_ptr<MediaServerLauncher>(new MediaServerLauncher());
        m_server->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
    }

    void whenServerLaunchedAndDataPrepared()
    {
        ASSERT_TRUE(m_server->start());
        m_cameraData.typeId = qnResTypePool->getResourceTypeByName("Camera")->getId();
        m_cameraData.physicalId = "physId";
        m_cameraData.vendor = "vendor";
        m_cameraData.id = nx::vms::api::CameraData::physicalIdToId(m_cameraData.physicalId);
        m_cameraData.mac = "mac";
        m_cameraData.model = "model";

        NX_TEST_API_POST(m_server.get(), "/ec2/saveCamera", m_cameraData);
    }

    void whenAuthSetViaResource()
    {
        QnNetworkResourcePtr camera;
        while (!camera)
        {
            camera = m_server->serverModule()->commonModule()
                ->resourcePool()->getResourceById<QnNetworkResource>(m_cameraData.id);
        }

        ASSERT_TRUE(camera);
        camera->setAuth("user", "password");
        camera->saveProperties();
    }

    void whenRequestedViaApi()
    {
        m_dataList.clear();
        const auto requestString = QString::fromLatin1("/ec2/getResourceParams?id=%1")
            .arg(m_cameraData.id.toSimpleString());
        NX_TEST_API_GET(m_server.get(), requestString, &m_dataList);
    }

    void thenItShouldBeObfuscated()
    {
        assertDataListEntry(ResourcePropertyKey::kCredentials, "user:******", true);
    }

    void whenRequestedViaResource()
    {
        m_dataList.clear();
        const auto auth = m_server->serverModule()->commonModule()->resourcePool()
            ->getResourceById<QnNetworkResource>(m_cameraData.id)->getAuth();
        m_dataList.push_back(vms::api::ResourceParamWithRefData(m_cameraData.id,
            ResourcePropertyKey::kCredentials, auth.user() + ":" + auth.password()));
    }

    void thenItShouldBeUncrypted()
    {
        assertDataListEntry(ResourcePropertyKey::kCredentials, "user:password", true);
    }

    void whenAuthSetViaApi()
    {
        vms::api::ResourceParamWithRefDataList dataList;
        dataList.push_back(vms::api::ResourceParamWithRefData(m_cameraData.id,
            ResourcePropertyKey::kCredentials, "user:password"));
        NX_TEST_API_POST(m_server.get(), "/ec2/setResourceParams", dataList);
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
    vms::api::CameraData m_cameraData;
    vms::api::ResourceParamWithRefDataList m_dataList;

    void assertDataListEntry(const QString& name, const QString& value, bool shouldBeFound)
    {
        const auto credentialsIt = std::find_if(m_dataList.cbegin(), m_dataList.cend(),
            [&name, &value](const auto& p)
            {
                return p.name == name && p.value == value;
            });

        if (shouldBeFound)
            ASSERT_NE(credentialsIt, m_dataList.cend());
        else
            ASSERT_EQ(credentialsIt, m_dataList.cend());
    }
};

TEST_F(CameraAuthUt, amendTransaction)
{
    checkAmendTransaction<vms::api::ResourceParamData>();
    checkAmendTransaction<vms::api::ResourceParamWithRefData>();
}

TEST_F(CameraAuthUt, amendOutputData)
{
    checkOutputDataAmending<vms::api::ResourceParamData>();
    checkOutputDataAmending<vms::api::ResourceParamWithRefData>();
}

TEST_F(CameraAuthFt, SetViaNetworkResource)
{
    whenServerLaunchedAndDataPrepared();
    whenAuthSetViaResource();

    whenRequestedViaApi();
    thenItShouldBeObfuscated();

    whenRequestedViaResource();
    thenItShouldBeUncrypted();
}

TEST_F(CameraAuthFt, SetViaAPI)
{
    whenServerLaunchedAndDataPrepared();
    whenAuthSetViaApi();

    whenRequestedViaApi();
    thenItShouldBeObfuscated();

    whenRequestedViaResource();
    thenItShouldBeUncrypted();
}

} // namespace nx::test
