#include <algorithm>
#include <gtest/gtest.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>
#include "../detail/update_server.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {
namespace test {

class TestProviderHandler: public AbstractAsyncRawDataProviderHandler
{
public:
    virtual void onGetUpdatesMetaInformationDone(ResultCode resultCode, QByteArray rawData) override
    {
        QnMutexLocker lock(&m_mutex);
        m_lastResultCode = resultCode;
        m_metaData = std::make_unique<QByteArray>(std::move(rawData));
        m_condition.wakeOne();;
    }

    virtual void onGetSpecificUpdateInformationDone(
        ResultCode resultCode,
        QByteArray rawData) override
    {
        QnMutexLocker lock(&m_mutex);
        m_lastResultCode = resultCode;
        m_specificData = std::make_unique<QByteArray>(std::move(rawData));
        m_condition.wakeOne();;
    }

    QByteArray metaData()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_metaData)
            m_condition.wait(lock.mutex());

        return *m_metaData;
    }

    QByteArray updateData()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_specificData)
            m_condition.wait(lock.mutex());

        return *m_specificData;
    }

    ResultCode lastResultCode() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_lastResultCode;
    }

    void reset()
    {
        m_metaData.reset();
        m_specificData.reset();
    }
private:
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;

    ResultCode m_lastResultCode = ResultCode::ok;
    std::unique_ptr<QByteArray> m_metaData;
    std::unique_ptr<QByteArray> m_specificData;
};

class AsyncJsonDataProvider: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        prepareDataProvider();
    }

    void whenGetUpdatesMetaInformationRequestIssued()
    {
        m_rawDataProvider->getUpdatesMetaInformation();
    }

    void whenChainOfUpdateRequestsIssued()
    {
        issueNextRequest();
    }

    void thenCorrectMetaDataIsProvided()
    {
        ASSERT_EQ(QByteArray(metaDataJson), m_providerHandler.metaData());
        ASSERT_EQ(ResultCode::ok, m_providerHandler.lastResultCode());
    }

    void thenCorrectUpdatesAreProvided()
    {
        assertResultCodes();
        assertResponses();
    }

private:
    struct UpdateResponse
    {
        ResultCode resultCode;
        QByteArray responseData;

        UpdateResponse(ResultCode resultCode, const QByteArray& responseData):
            resultCode(resultCode),
            responseData(responseData)
        {}
    };

    AbstractAsyncRawDataProviderPtr m_rawDataProvider;
    TestProviderHandler m_providerHandler;
    info::test::detail::UpdateServer m_updateServer;
    size_t m_updateRequestCount = 0;
    QList<UpdateResponse> m_updateResponses;

    void prepareDataProvider()
    {
        nx::utils::Url baseUrl;
        baseUrl.setScheme("http");
        baseUrl.setHost(m_updateServer.address().address.toString());
        baseUrl.setPort(m_updateServer.address().port);

        m_rawDataProvider = RawDataProviderFactory::create(baseUrl.toString(), &m_providerHandler);
        ASSERT_TRUE((bool) m_rawDataProvider);
    }

    void issueNextRequest()
    {
        if (m_updateRequestCount >= updateTestDataList.size())
            return;

        m_providerHandler.reset();
        const UpdateTestData& updateTestData = updateTestDataList[m_updateRequestCount];
        m_rawDataProvider->getSpecificUpdateData(
            updateTestData.customization,
            updateTestData.version);

        processResponse();
    }

    void processResponse()
    {
        m_updateResponses.append(
            UpdateResponse(
                m_providerHandler.lastResultCode(),
                m_providerHandler.updateData()));
        ++m_updateRequestCount;
        issueNextRequest();
    }

    void assertResultCodes()
    {
        bool allResultsCodesAreOk = std::all_of(
            m_updateResponses.cbegin(),
            m_updateResponses.cend(),
            [](const UpdateResponse& updateResponse)
            {
                return updateResponse.resultCode == ResultCode::ok;
            });
        ASSERT_TRUE(allResultsCodesAreOk);
    }

    void assertResponses()
    {
        ASSERT_EQ(static_cast<int>(updateTestDataList.size()), m_updateResponses.size());
        for (size_t i = 0; i < updateTestDataList.size(); ++i)
            assertResponse(i);
    }

    void assertResponse(size_t i)
    {
        const auto& updateTestData = updateTestDataList[i];
        const auto updateResponse = m_updateResponses[static_cast<int>(i)];
        ASSERT_EQ(updateTestData.json, updateResponse.responseData);
    }
};

TEST_F(AsyncJsonDataProvider, MetaDataProvided)
{
    whenGetUpdatesMetaInformationRequestIssued();
    thenCorrectMetaDataIsProvided();
}

TEST_F(AsyncJsonDataProvider, UpdateDataProvided)
{
    whenChainOfUpdateRequestsIssued();
    thenCorrectUpdatesAreProvided();
}

} // namespace test
} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
