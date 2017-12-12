#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/utils/service.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace utils {
namespace test {

namespace {

class ServiceSettings:
    public AbstractServiceSettings
{
public:
    ServiceSettings(QString dataDir):
        m_dataDir(dataDir)
    {
    }

    virtual void load(int /*argc*/, const char** /*argv*/) override
    {
    }

    virtual bool isShowHelpRequested() const override
    {
        return false;
    }

    virtual void printCmdLineArgsHelp() override
    {
    }

    virtual QString dataDir() const override
    {
        return m_dataDir;
    }

    virtual utils::log::Settings logging() const override
    {
        return utils::log::Settings();
    }

private:
    QString m_dataDir;
};

class TestService:
    public utils::Service
{
    using base_type = utils::Service;

public:
    TestService(const QString& dataDir):
        base_type(0, nullptr, "utils_ut"),
        m_dataDir(dataDir)
    {
    }

protected:
    virtual std::unique_ptr<AbstractServiceSettings> createSettings() override
    {
        return std::make_unique<ServiceSettings>(m_dataDir);
    }

    virtual int serviceMain(const AbstractServiceSettings& settings) override
    {
        return runMainLoop();
    }

private:
    QString m_dataDir;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class Service:
    public ::testing::Test,
    public TestWithTemporaryDirectory
{
public:
    Service():
        TestWithTemporaryDirectory("nx_utils_ut", QString())
    {
    }

protected:
    void givenStartedService()
    {
        using namespace std::placeholders;

        m_serviceContext = std::make_unique<ServiceContext>(testDataDir());
        m_serviceContext->service.setOnAbnormalTerminationDetected(
            std::bind(&Service::saveAbnormalTerminationReport, this, _1));
        nx::utils::promise<void> serviceStarted;
        m_serviceContext->service.setOnStartedEventHandler(
            [&serviceStarted](bool /*isStarted*/)
            {
                serviceStarted.set_value();
            });
        m_serviceContext->mainThread = nx::utils::thread(
            [service = &m_serviceContext->service]() { service->exec(); });
        serviceStarted.get_future().wait();
    }

    void givenServiceNotCorrectlyStopped()
    {
        givenStartedService();
        // Just leaving service started.
        // It is true, that service has not been stopped properly in that case.

        m_serviceContextBak.swap(m_serviceContext);
    }

    void whenRestartService()
    {
        m_serviceContext.reset();
        givenStartedService();
    }

    void thenAbnormalTerminationIsReported()
    {
        m_abnormalTerminationReports.pop();
    }

    void thenAbnormalTerminationIsNotReported()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_TRUE(m_abnormalTerminationReports.empty());
    }

private:
    struct ServiceContext
    {
        TestService service;
        nx::utils::thread mainThread;

        ServiceContext(const QString& dataDir):
            service(dataDir)
        {
        }

        ~ServiceContext()
        {
            service.pleaseStop();
            mainThread.join();
        }
    };

    std::unique_ptr<ServiceContext> m_serviceContext;
    std::unique_ptr<ServiceContext> m_serviceContextBak;
    std::vector<const char*> m_args;
    nx::utils::SyncQueue<ServiceStartInfo> m_abnormalTerminationReports;

    void saveAbnormalTerminationReport(ServiceStartInfo serviceStartInfo)
    {
        m_abnormalTerminationReports.push(serviceStartInfo);
    }
};

TEST_F(Service, abnormal_termination_is_reported_if_start_info_file_is_present)
{
    givenServiceNotCorrectlyStopped();
    whenRestartService();
    thenAbnormalTerminationIsReported();
}

TEST_F(Service, abnormal_termination_is_not_reported_after_correct_restart)
{
    givenStartedService();
    whenRestartService();
    thenAbnormalTerminationIsNotReported();
}

} // namespace test
} // namespace utils
} // namespace nx
