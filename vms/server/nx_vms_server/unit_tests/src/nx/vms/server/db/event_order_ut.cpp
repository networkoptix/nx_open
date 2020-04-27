#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>

#include <test_support/mediaserver_launcher.h>
#include <media_server/media_server_module.h>
#include <api/helpers/event_log_request_data.h>
#include <nx/vms/event/actions/common_action.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <api/test_api_requests.h>
#include <api/server_rest_connection.h>
#include <api/helpers/event_log_multiserver_request_data.h>

#include <common/common_module.h>

#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/barrier_handler.h>

using namespace std::chrono;
using namespace std::literals::chrono_literals;

using namespace nx::test;
using namespace nx::vms::event;

namespace nx::vms::server::db::test {

static bool isOrdered(const ActionData& first, const ActionData& second, Qt::SortOrder sortOrder)
{
    if (sortOrder == Qt::SortOrder::AscendingOrder)
    {
        return first.eventParams.eventTimestampUsec
            < second.eventParams.eventTimestampUsec;
    }

    return first.eventParams.eventTimestampUsec
        > second.eventParams.eventTimestampUsec;
}

class EventOrderTest: public ::testing::Test
{
protected:

    void givenSingleServer()
    {
        makeServers(1);
    }

    void givenMultipleServers()
    {
        makeServers(5);
        connectAllServers();
        waitUntilConnectionEstablished();
    }

    void afterReceivingMultipleUnorderedInTimeEvents()
    {
        static constexpr int kStepUs = 1000;
        for (int i = 0; i < m_servers.size(); ++i)
        {
            QnServerDb* serverDb = m_servers[i]->serverModule()->serverDb();

            const int shiftUs = i * 1000;

            createAndSaveRangeOfEvents(serverDb, shiftUs + 1000, shiftUs + 5000, kStepUs);
            createAndSaveRangeOfEvents(serverDb, shiftUs + 10000, shiftUs + 6000, kStepUs);
            createAndSaveRangeOfEvents(serverDb, shiftUs + 11000, shiftUs + 16000, kStepUs);
        }
    }

    void makeSureDatabaseReturnsOrderedEventList(Qt::SortOrder sortOrder)
    {
        QnEventLogFilterData filter;
        filter.period.setStartTime(milliseconds::zero());
        filter.period.setEndTime(milliseconds::max());

        QnServerDb* serverDb = m_servers[0]->serverModule()->serverDb();

        const ActionDataList result = serverDb->getActions(filter, sortOrder);
        ASSERT_TRUE(std::is_sorted(
            result.cbegin(),
            result.cend(),
            [sortOrder](const ActionData& prev, const ActionData& current)
            {
                return isOrdered(prev, current, sortOrder);
            }));
    }

    void makeSureEachServerReturnsOrderedEventList(Qt::SortOrder sortOrder)
    {
        for (const std::unique_ptr<MediaServerLauncher>& server: m_servers)
        {
            QByteArray response;
            NX_TEST_API_GET(
                server.get(),
                lm("/ec2/getEvents?from=0&to=%1&sortOrder=%2").args(
                    milliseconds::max().count(),
                    sortOrder == Qt::SortOrder::AscendingOrder ? "asc" : "desc"),
                &response);

            QJsonObject deserialized;
            ASSERT_TRUE(QJson::deserialize(response, &deserialized));

            ActionDataList actionDataList;
            ASSERT_TRUE(QJson::deserialize(deserialized, "reply", &actionDataList));

            ASSERT_TRUE(std::is_sorted(
                actionDataList.cbegin(),
                actionDataList.cend(),
                [sortOrder](const ActionData& prev, const ActionData& current)
                {
                    return isOrdered(prev, current, sortOrder);
                }));
        }
    }

private:
    void makeServers(int serverCount)
    {
        for (int i = 0; i < serverCount; ++i)
        {
            auto server = std::make_unique<MediaServerLauncher>();
            ASSERT_TRUE(server->start());
            m_servers.push_back(std::move(server));
        }
    }

    void connectAllServers()
    {
        for (int i = 0; i < m_servers.size(); ++i)
        {
            if (i < m_servers.size() - 1)
                m_servers[i]->connectTo(m_servers[i + 1].get());
        }
    }

    void waitUntilConnectionEstablished()
    {
        static const std::chrono::seconds kSyncWaitTimeout(30);
        bool isConnectionEstablished = false;
        nx::utils::ElapsedTimer timer;
        timer.restart();

        do
        {
            isConnectionEstablished = std::all_of(
                m_servers.cbegin(),
                m_servers.cend(),
                [this](const auto& server)
                {
                    nx::vms::api::MediaServerDataList serverDataList;
                    auto ecConnection = server->commonModule()->ec2Connection();
                    ecConnection->getMediaServerManager(Qn::kSystemAccess)
                        ->getServersSync(&serverDataList);

                    return serverDataList.size() == m_servers.size();
                });

            if (!isConnectionEstablished)
                std::this_thread::sleep_for(10ms);

        } while (!isConnectionEstablished && timer.elapsed() < kSyncWaitTimeout);

        ASSERT_TRUE(isConnectionEstablished);
    }

    AbstractActionPtr createAction(int64_t timestampUs)
    {
        EventParameters parameters;
        parameters.eventTimestampUsec = timestampUs;

        return CommonAction::create(ActionType::bookmarkAction, parameters);
    }

    void createAndSaveRangeOfEvents(
        QnServerDb* serverDb,
        int64_t rangeStartTimestampUs,
        int64_t rangeEndTimestampUs,
        int stepUs)
    {
        if (rangeStartTimestampUs > rangeEndTimestampUs)
            stepUs = -stepUs;

        auto isFullRangeGenerated =
            [](int64_t rangeStartTimestampUs,
                int64_t rangeEndTimestampUs,
                int64_t currentTimestampUs)
            {
                return rangeStartTimestampUs <= rangeEndTimestampUs
                    ? currentTimestampUs > rangeEndTimestampUs
                    : currentTimestampUs < rangeEndTimestampUs;
            };

        for (int eventTimestampUs = rangeStartTimestampUs;
            !isFullRangeGenerated(rangeStartTimestampUs, rangeEndTimestampUs, eventTimestampUs);
            eventTimestampUs += stepUs)
        {
            AbstractActionPtr action = createAction(eventTimestampUs);
            serverDb->saveActionToDB(action);
        }
    }

private:
    std::vector<std::unique_ptr<MediaServerLauncher>> m_servers;
};

TEST_F(EventOrderTest, eventsAreOrderedByTimestamp)
{
    givenSingleServer();
    afterReceivingMultipleUnorderedInTimeEvents();
    makeSureDatabaseReturnsOrderedEventList(Qt::AscendingOrder);
    makeSureDatabaseReturnsOrderedEventList(Qt::DescendingOrder);
}

TEST_F(EventOrderTest, eventsFromMultipleServersAreOrderedByTimestamp)
{
    givenMultipleServers();
    afterReceivingMultipleUnorderedInTimeEvents();
    makeSureEachServerReturnsOrderedEventList(Qt::AscendingOrder);
    makeSureEachServerReturnsOrderedEventList(Qt::AscendingOrder);
}

} // namespace nx::vms::server::db::test
