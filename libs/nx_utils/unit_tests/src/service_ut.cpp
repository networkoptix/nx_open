// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/utils/service.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/time.h>

namespace nx {
namespace utils {
namespace test {

namespace {

class ServiceSettings: public AbstractServiceSettings
{
public:
    ServiceSettings(QString dataDir): m_dataDir(dataDir) {}

    virtual void load(int /*argc*/, const char** /*argv*/) override {}

    virtual bool isShowHelpRequested() const override { return false; }

    virtual void printCmdLineArgsHelp() override {}

    virtual QString dataDir() const override { return m_dataDir; }

    virtual utils::log::Settings logging() const override { return utils::log::Settings(); }

private:
    QString m_dataDir;
};

class TestService: public utils::Service
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

    virtual int serviceMain(const AbstractServiceSettings& /*settings*/) override
    {
        return runMainLoop();
    }

private:
	QString m_dataDir;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class Service: public ::testing::Test, public TestWithTemporaryDirectory
{
protected:
    void givenStartedService()
    {
		using namespace std::placeholders;

		if (!m_initialServiceStartTime)
			m_initialServiceStartTime = std::chrono::system_clock::now();

		m_serviceContext = std::make_unique<ServiceContext>(testDataDir());
		m_serviceContext->service.setOnAbnormalTerminationDetected(
			std::bind(&Service::saveAbnormalTerminationReport, this, _1));

		// Setting this event to check for m_serviceStartedEvent.empty()
		m_serviceContext->service.setOnStartedEventHandler(
			[this](bool isStarted) { m_serviceStartedEvent.push(isStarted); });

		m_serviceContext->mainThread = nx::utils::thread(
			[serviceContext = m_serviceContext.get()]()
			{
				serviceContext->serviceStarted = true;
				serviceContext->service.exec();
				serviceContext->execFinished = true;
			});
		ASSERT_TRUE(m_serviceStartedEvent.pop());
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

    void whenServiceRestartsItself()
    {
		m_serviceContext->service.setOnStartedEventHandler(
			[this](bool isStarted) { m_serviceStartedEvent.push(isStarted); });

        // Service::restart() can be called from any thread, so just call it here
        m_serviceContext->service.restart();
    }

    void thenAbnormalTerminationIsReported()
    {
        m_lastReceivedReport = m_abnormalTerminationReports.pop();
    }

    void andReportedTimeIsCorrect()
    {
        using namespace std::chrono;

        ASSERT_GE(
            m_lastReceivedReport.startTime, nx::utils::floor<seconds>(*m_initialServiceStartTime));
        ASSERT_LE(m_lastReceivedReport.startTime, system_clock::now());
    }

    void thenAbnormalTerminationIsNotReported()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_TRUE(m_abnormalTerminationReports.empty());
    }

    void thenServiceIsRestarted()
	{
		ASSERT_TRUE(m_serviceStartedEvent.pop());
	}

    void andServiceIsStillExecuting()
	{
		ASSERT_FALSE(m_serviceContext->execFinished);
	}

    void whenServiceIsStopped()
	{
		m_serviceContext->service.pleaseStop();
	}

	void thenServiceExecutionFinishes()
	{
		while (!m_serviceContext->execFinished)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

private:
    struct ServiceContext
    {
        TestService service;
        nx::utils::thread mainThread;
        std::atomic_bool execFinished = false;
		std::atomic_bool serviceStarted = false;

        ServiceContext(const QString& dataDir):
			service(dataDir)
		{
		}

        ~ServiceContext()
        {
			if (!serviceStarted)
				return;

			service.pleaseStop();
            mainThread.join();
        }
    };

    nx::utils::SyncQueue<bool> m_serviceStartedEvent;
    std::unique_ptr<ServiceContext> m_serviceContext;
    std::unique_ptr<ServiceContext> m_serviceContextBak;
    std::vector<const char*> m_args;
    nx::utils::SyncQueue<ServiceStartInfo> m_abnormalTerminationReports;
    ServiceStartInfo m_lastReceivedReport;
    std::optional<std::chrono::system_clock::time_point> m_initialServiceStartTime;

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
    andReportedTimeIsCorrect();
}

TEST_F(Service, abnormal_termination_is_not_reported_after_correct_restart)
{
    givenStartedService();
    whenRestartService();
    thenAbnormalTerminationIsNotReported();
}

TEST_F(Service, multiple_restarts)
{
    for (int i = 0; i < 10; ++i)
    {
        givenStartedService();

        whenServiceRestartsItself();

		thenServiceIsRestarted();
        thenAbnormalTerminationIsNotReported();

		andServiceIsStillExecuting();
    }
}

TEST_F(Service, stops_after_restart)
{
	givenStartedService();

	whenServiceRestartsItself();

	thenServiceIsRestarted();
	thenAbnormalTerminationIsNotReported();
	andServiceIsStillExecuting();

	whenServiceIsStopped();
	thenServiceExecutionFinishes();
}

} // namespace test
} // namespace utils
} // namespace nx
