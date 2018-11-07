#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/resource_data.h>
#include <rest/server/json_rest_result.h>
#include <server_query_processor.h>

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
        qDebug() << "Original:" << originalTran.params.name << originalTran.params.value;
        qDebug() << "Amended:" << processedTran.params.name << processedTran.params.value;
        ASSERT_NE(originalTran.params, processedTran.params);
    }

    template<typename Data>
    void thenItShouldNOTBeAmended(
        const ec2::QnTransaction<Data>& originalTran,
        const ec2::QnTransaction<Data>& processedTran)
    {
        ASSERT_EQ(originalTran.params, processedTran.params);
        qDebug() << "Original:" << originalTran.params.name << originalTran.params.value;
        qDebug() << "Amended:" << processedTran.params.name << processedTran.params.value;
    }
};

class CameraAuth: public ::testing::Test
{
protected:
    CameraAuth()
    {
        m_server = std::unique_ptr<MediaServerLauncher>(new MediaServerLauncher());
        m_server->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
    }

    void whenServerLaunched()
    {
        ASSERT_TRUE(m_server->start());
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
};

TEST_F(CameraAuthUt, amendTransaction_ParamData)
{
    auto originalTran = createTran<vms::api::ResourceParamData>(
        Qn::CAMERA_CREDENTIALS_PARAM_NAME,
        "testValue");
    auto amendedTran = whenTransactionProcessed(originalTran);
    thenItShouldBeAmended(originalTran, amendedTran);

    originalTran = createTran<vms::api::ResourceParamData>(
        Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME,
        "testValue");
    amendedTran = whenTransactionProcessed(originalTran);
    thenItShouldBeAmended(originalTran, amendedTran);

    originalTran = createTran<vms::api::ResourceParamData>(
        "some_other_name",
        "testValue");
    amendedTran = whenTransactionProcessed(originalTran);
    thenItShouldNOTBeAmended(originalTran, amendedTran);
}

TEST_F(CameraAuthUt, amendTransaction_ParamWithRefData)
{
    auto originalTran = createTran<vms::api::ResourceParamWithRefData>(
        Qn::CAMERA_CREDENTIALS_PARAM_NAME,
        "testValue");
    auto amendedTran = whenTransactionProcessed(originalTran);
    thenItShouldBeAmended(originalTran, amendedTran);

    originalTran = createTran<vms::api::ResourceParamWithRefData>(
        Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME,
        "testValue");
    amendedTran = whenTransactionProcessed(originalTran);
    thenItShouldBeAmended(originalTran, amendedTran);

    originalTran = createTran<vms::api::ResourceParamWithRefData>(
        "some_other_name",
        "testValue");
    amendedTran = whenTransactionProcessed(originalTran);
    thenItShouldNOTBeAmended(originalTran, amendedTran);}

TEST_F(CameraAuth, SetGetViaNetworkResource)
{
    // whenServerLaunched();
    // whenCameraSaved();
    // whenAuthSetViaResource();
    // whenAuthRequestedViaResource();
    // thenItShouldBeOk();
}

TEST_F(CameraAuth, SetViaAPIGetViaNetworkResource)
{
}

TEST_F(CameraAuth, SetGetViaAPI)
{
}

} // namespace nx::test
