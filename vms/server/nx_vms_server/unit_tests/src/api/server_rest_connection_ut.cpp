#include <gtest/gtest.h>

#include <api/server_rest_connection.h>
#include <mediaserver_ini.h>
#include <nx/utils/test_support/run_test.h>
#include <test_support/mediaserver_launcher.h>

namespace nx::test {

using namespace ::rest;

constexpr size_t kRequestsMultiplier(100);
constexpr std::chrono::seconds kTimeout(10);
constexpr std::chrono::milliseconds kClientPoolTimeouts(100);

class HandleKeeper
{
public:
    void add(Handle handle)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        ASSERT_TRUE(m_started.insert(handle).second) << handle;
    }

    void notify(Handle handle)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        ASSERT_TRUE(m_notified.insert(handle).second) << handle;
        m_condition.wakeOne();
    }

    ~HandleKeeper()
    {
        const auto startTime = std::chrono::steady_clock::now();
        std::chrono::milliseconds timeLeft(0);
        NX_MUTEX_LOCKER lock(&m_mutex);
        do
        {
            for (auto it = m_started.begin(); it != m_started.end();)
            {
                if (m_notified.erase(*it) == 0)
                    ++it;
                else
                    it = m_started.erase(it);
            }

            const auto timePassed = (std::chrono::steady_clock::now() - startTime);
            timeLeft = std::chrono::duration_cast<std::chrono::milliseconds>(kTimeout - timePassed);
        } while (
            !m_started.empty() && timeLeft.count() > 0 && m_condition.wait(&m_mutex, timeLeft));

        EXPECT_TRUE(m_started.empty()) << containerString(m_started).toStdString();
        EXPECT_TRUE(m_notified.empty()) << containerString(m_notified).toStdString();
        EXPECT_GT(timeLeft.count(), 0);
    }

private:
    nx::utils::Mutex m_mutex;
    nx::utils::WaitCondition m_condition;
    std::set<Handle> m_started;
    std::set<Handle> m_notified;
};

class FtServerRestConnection: public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        iniTweaks = std::make_unique<nx::utils::test::IniConfigTweaks>();
        iniTweaks->set(&ini().enableApiDebug, true);

        server = std::make_unique<MediaServerLauncher>();
        ASSERT_TRUE(server->start());

        common = server->commonModule();
        common->httpClientPool()->setDefaultTimeouts(
            kClientPoolTimeouts, kClientPoolTimeouts, kClientPoolTimeouts);
    }

    static void TearDownTestCase()
    {
        server.reset();
        iniTweaks.reset();
    }

    FtServerRestConnection():
        connection(std::make_unique<ServerConnection>(common, common->moduleGUID()))
    {
    }

protected:
    static std::unique_ptr<nx::utils::test::IniConfigTweaks> iniTweaks;
    static std::unique_ptr<MediaServerLauncher> server;
    static QnCommonModule* common;

    const std::unique_ptr<ServerConnection> connection;
    HandleKeeper handlerKeeper;
};

std::unique_ptr<nx::utils::test::IniConfigTweaks> FtServerRestConnection::iniTweaks;
std::unique_ptr<MediaServerLauncher> FtServerRestConnection::server;
QnCommonModule* FtServerRestConnection::common;

TEST_F(FtServerRestConnection, getStatistics)
{
    for (size_t i = 0; i != kRequestsMultiplier; ++i)
    {
        handlerKeeper.add(connection->getStatistics(
            [&](bool success, rest::Handle handle, auto result)
            {
                EXPECT_TRUE(success);
                EXPECT_TRUE(result.error == QnRestResult::Error::NoError);
                handlerKeeper.notify(handle);
            }));
    }
}

TEST_F(FtServerRestConnection, timeout)
{
    for (size_t i = 0; i != kRequestsMultiplier; ++i)
    {
        handlerKeeper.add(connection->debug(
            "delayS", QString::number(10),
            [&](bool success, rest::Handle handle, auto /*result*/)
            {
                EXPECT_FALSE(success);
                handlerKeeper.notify(handle);
            }));
    }
}

} // namespace nx::test
