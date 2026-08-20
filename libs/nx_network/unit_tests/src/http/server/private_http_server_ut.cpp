// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/server/metrics_request_handler.h>
#include <nx/network/http/server/private_http_server.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/prometheus/registry.h>
#include <nx/reflect/json.h>

namespace nx::network::http::server::test {

namespace {

std::optional<double> requestCount(
    const std::string& scraped,
    const std::string& serverName,
    const std::string& route)
{
    const std::string serverNameLabel =
        HttpRequestMetrics::kLabelServerName + "=\"" + serverName + "\"";
    const std::string routeLabel = "http_route=\"" + route + "\"";

    std::istringstream stream(scraped);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.rfind("http_server_request_duration_seconds_count", 0) == 0
            && line.find(serverNameLabel) != std::string::npos
            && line.find(routeLabel) != std::string::npos)
        {
            return std::stod(line.substr(line.rfind(' ') + 1));
        }
    }
    return std::nullopt;
}

/**
 * Starts a listener for the liveness self-request to reach and points the health monitor at it.
 */
std::unique_ptr<MultiEndpointServer> startApplicationServer(
    rest::MessageDispatcher* dispatcher,
    HealthMonitor* healthMonitor)
{
    Settings settings;
    settings.endpoints.push_back(SocketAddress("127.0.0.1:0"));

    auto server = Builder::buildOrThrow(settings, dispatcher);
    EXPECT_TRUE(server->listen());
    healthMonitor->setLivenessTarget(server->endpoints().front(), /*isSecure*/ false);

    return server;
}

} // namespace

class PrivateHttpServerTest: public ::testing::Test
{
protected:
    ~PrivateHttpServerTest()
    {
        m_privateServer.pleaseStopSync();
        if (m_applicationServer)
            m_applicationServer->pleaseStopSync();
    }

    void givenApplicationServer()
    {
        m_applicationServer =
            startApplicationServer(&m_applicationDispatcher, &m_privateServer.healthMonitor());
    }

    std::string scrape()
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        client.setTimeouts(AsyncClient::kInfiniteTimeouts);
        const auto port = m_privateServer.endpoints().front().port;
        EXPECT_TRUE(client.doGet(nx::Url(nx::format("http://127.0.0.1:%1/metrics").args(port))));
        EXPECT_EQ(StatusCode::ok, client.response()->statusLine.statusCode);
        const auto body = client.fetchEntireMessageBody();
        return body ? body->toStdString() : std::string();
    }

    nx::prometheus::Registry m_registry{"testService", "dev"};
    PrivateHttpServer m_privateServer{&m_registry, SocketAddress("127.0.0.1:0")};

private:
    rest::MessageDispatcher m_applicationDispatcher;
    std::unique_ptr<MultiEndpointServer> m_applicationServer;
};

TEST_F(PrivateHttpServerTest, scrape_exposes_public_server_request_count)
{
    // The production topology: a public-facing server and the private server share one metrics
    // registry, the liveness self-request of the private server goes to the public one, and the
    // public server's traffic is observed by scraping the private /metrics.
    Settings publicSettings;
    publicSettings.endpoints.push_back(SocketAddress("127.0.0.1:0"));

    rest::MessageDispatcher publicDispatcher;
    publicDispatcher.registerRequestProcessorFunc(
        Method::get,
        "/ping",
        [](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
        {
            completionHandler(StatusCode::ok);
        });

    auto publicServer = Builder::buildOrThrow(
        publicSettings,
        &publicDispatcher,
        &m_registry,
        /*serverName*/ "public");
    ASSERT_TRUE(publicServer->listen());

    m_privateServer.healthMonitor().setLivenessTarget(
        publicServer->endpoints().front(), /*isSecure*/ false);
    ASSERT_TRUE(m_privateServer.listen());

    // A single request to the public server.
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        client.setTimeouts(AsyncClient::kInfiniteTimeouts);
        const auto port = publicServer->endpoints().front().port;
        ASSERT_TRUE(client.doGet(nx::Url(nx::format("http://127.0.0.1:%1/ping").args(port))));
        ASSERT_EQ(StatusCode::ok, client.response()->statusLine.statusCode);
    }

    // The duration histogram (whose _count is the request total) is recorded server-side once
    // the request completes, which may lag the client's receipt of the response. Scrape until
    // the count series of the request appears. It is looked up by route, so the liveness
    // self-requests - which go to an unregistered path and are recorded with an empty route -
    // cannot be counted among them.
    std::optional<double> count;
    for (int i = 0; i < 100; ++i)
    {
        count = requestCount(scrape(), /*serverName*/ "public", /*route*/ "/ping");
        if (count == 1.0)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_EQ(count, 1.0);
}

TEST_F(PrivateHttpServerTest, diagnostic_endpoints_are_served)
{
    givenApplicationServer();
    ASSERT_TRUE(m_privateServer.listen());

    HttpClient client{ssl::kAcceptAnyCertificate};
    client.setTimeouts(AsyncClient::kInfiniteTimeouts);
    const auto port = m_privateServer.endpoints().front().port;

    for (const auto& path: {maintenance::kMallocInfo, maintenance::kDebugCounters})
    {
        ASSERT_TRUE(client.doGet(nx::Url(nx::format("http://127.0.0.1:%1%2").args(port, path))));
        EXPECT_NE(StatusCode::notFound, client.response()->statusLine.statusCode) << path;
    }
}

//-------------------------------------------------------------------------------------------------
// Health endpoints.

namespace {

static constexpr char kProbeName[] = "dependency";

} // namespace

class PrivateHttpServerHealthTest: public ::testing::Test
{
protected:
    // The shortest the monitor accepts, to keep the tests quick: the staleness limit is three
    // of these.
    static constexpr std::chrono::milliseconds kCheckPeriod = HealthMonitor::kMinCheckPeriod;

    ~PrivateHttpServerHealthTest()
    {
        m_privateServer.pleaseStopSync();
        stopApplicationServer();
    }

    void givenReadinessProbe()
    {
        m_privateServer.healthMonitor().registerReadinessProbe(
            kProbeName,
            [this](HealthMonitor::ProbeHandler done)
            {
                std::lock_guard lock(m_probeMutex);
                if (!m_probeAnswers)
                    return; //< The handler is dropped: this probe never reports a result.

                done(m_probeError);
            });
    }

    void givenApplicationServer()
    {
        Settings settings;
        settings.endpoints.push_back(SocketAddress("127.0.0.1:0"));

        // Counts the self-requests and answers them with an error status: it is the response
        // itself that liveness accepts as proof of a live listener, whatever it says.
        m_applicationDispatcher.registerRequestProcessorFunc(
            Method::get,
            HealthMonitor::kLivenessSelfRequestPath,
            [this](RequestContext /*requestContext*/, RequestProcessedHandler completionHandler)
            {
                ++m_selfRequestCount;
                completionHandler(StatusCode::forbidden);
            });

        m_applicationServer =
            startApplicationServer(&m_applicationDispatcher, &m_privateServer.healthMonitor());
    }

    /**
     * Waits for the application listener to answer a self-request. Liveness reports ok before the
     * first check completes, so asserting it only means something once one has run.
     */
    void whenApplicationListenerAnswered()
    {
        while (m_selfRequestCount == 0)
            std::this_thread::sleep_for(kCheckPeriod);
    }

    void whenStartPrivateServer()
    {
        // A liveness target is required before the start; the tests that do not care which
        // listener answers get this one.
        if (!m_applicationServer)
            givenApplicationServer();

        m_privateServer.healthMonitor().setCheckPeriod(kCheckPeriod);
        ASSERT_TRUE(m_privateServer.listen());
    }

    void whenProbeReports(std::optional<std::string> error)
    {
        std::lock_guard lock(m_probeMutex);
        m_probeError = std::move(error);
        m_probeAnswers = true;
    }

    void whenProbeStopsAnswering()
    {
        std::lock_guard lock(m_probeMutex);
        m_probeAnswers = false;
    }

    void stopApplicationServer()
    {
        if (!m_applicationServer)
            return;

        m_applicationServer->pleaseStopSync();
        m_applicationServer.reset();
    }

    HealthReply thenHealthIs(const std::string& path, StatusCode::Value expectedStatusCode)
    {
        auto response = doGet(path);
        while (response.first != expectedStatusCode)
        {
            std::this_thread::sleep_for(kCheckPeriod);
            response = doGet(path);
        }

        HealthReply reply;
        EXPECT_TRUE(nx::reflect::json::deserialize(response.second, &reply))
            << "body: " << response.second;

        return reply;
    }

    /** The reason reported for a check, or a text that reads well in a failed assertion. */
    std::string failureReason(const HealthReply& reply, const std::string& checkName) const
    {
        if (!reply.failed || !reply.failed->contains(checkName))
            return "(nothing reported for " + checkName + ")";

        return reply.failed->at(checkName);
    }

    std::pair<StatusCode::Value, std::string> doGet(const std::string& path)
    {
        HttpClient client{ssl::kAcceptAnyCertificate};
        client.setTimeouts(AsyncClient::kInfiniteTimeouts);

        const auto port = m_privateServer.endpoints().front().port;
        if (!client.doGet(nx::Url(nx::format("http://127.0.0.1:%1%2").args(port, path))))
            return {StatusCode::undefined, std::string()};

        const auto body = client.fetchEntireMessageBody();
        return {
            (StatusCode::Value) client.response()->statusLine.statusCode,
            body ? body->toStdString() : std::string()};
    }

protected:
    nx::prometheus::Registry m_registry{"testService", "dev"};
    PrivateHttpServer m_privateServer{&m_registry, SocketAddress("127.0.0.1:0")};

private:
    rest::MessageDispatcher m_applicationDispatcher;
    std::unique_ptr<MultiEndpointServer> m_applicationServer;
    std::atomic<int> m_selfRequestCount{0};
    std::mutex m_probeMutex;
    std::optional<std::string> m_probeError;
    bool m_probeAnswers = true;
};

TEST_F(PrivateHttpServerHealthTest, health_endpoints_are_served_with_no_probes_registered)
{
    whenStartPrivateServer();

    EXPECT_EQ(HealthReply::Status::ok,
        thenHealthIs(PrivateHttpServer::kReadinessPath, StatusCode::ok).status);
    EXPECT_EQ(HealthReply::Status::ok,
        thenHealthIs(PrivateHttpServer::kLivenessPath, StatusCode::ok).status);
    EXPECT_EQ(R"({"status":"ok"})", doGet(PrivateHttpServer::kReadinessPath).second);
}

TEST_F(PrivateHttpServerHealthTest, readiness_reports_the_failing_probe)
{
    givenReadinessProbe();
    whenProbeReports("the database is unreachable");
    whenStartPrivateServer();

    const auto reply = thenHealthIs(
        PrivateHttpServer::kReadinessPath, StatusCode::serviceUnavailable);
    EXPECT_EQ(HealthReply::Status::unavailable, reply.status);
    EXPECT_EQ("the database is unreachable", failureReason(reply, kProbeName));

    EXPECT_EQ(HealthReply::Status::ok,
        thenHealthIs(PrivateHttpServer::kLivenessPath, StatusCode::ok).status);
}

TEST_F(PrivateHttpServerHealthTest, readiness_recovers_once_the_probe_passes)
{
    givenReadinessProbe();
    whenProbeReports("the database is unreachable");
    whenStartPrivateServer();
    thenHealthIs(PrivateHttpServer::kReadinessPath, StatusCode::serviceUnavailable);

    whenProbeReports(std::nullopt);

    EXPECT_EQ(HealthReply::Status::ok,
        thenHealthIs(PrivateHttpServer::kReadinessPath, StatusCode::ok).status);
}

TEST_F(PrivateHttpServerHealthTest, readiness_reports_a_probe_that_has_not_answered_yet)
{
    givenReadinessProbe();
    whenProbeStopsAnswering();
    whenStartPrivateServer();

    const auto reply = thenHealthIs(
        PrivateHttpServer::kReadinessPath, StatusCode::serviceUnavailable);
    EXPECT_EQ("not checked yet", failureReason(reply, kProbeName));
}

TEST_F(PrivateHttpServerHealthTest, readiness_recovers_from_a_probe_that_dropped_the_handler)
{
    givenReadinessProbe();
    whenProbeStopsAnswering(); //< Which drops the handler rather than holding it.
    whenStartPrivateServer();
    thenHealthIs(PrivateHttpServer::kReadinessPath, StatusCode::serviceUnavailable);

    whenProbeReports(std::nullopt);

    // A dropped check is made again by the next cycle. Were it left counted as running, this
    // dependency would never be looked at again and the service would stay unready for good.
    EXPECT_EQ(HealthReply::Status::ok,
        thenHealthIs(PrivateHttpServer::kReadinessPath, StatusCode::ok).status);
}

TEST_F(PrivateHttpServerHealthTest, readiness_reports_a_stuck_probe_as_stale)
{
    givenReadinessProbe();
    whenProbeReports(std::nullopt);
    whenStartPrivateServer();
    thenHealthIs(PrivateHttpServer::kReadinessPath, StatusCode::ok);

    // The probe stops reporting, so nothing refreshes its result, and it should become stale.
    whenProbeStopsAnswering();

    const auto reply = thenHealthIs(
        PrivateHttpServer::kReadinessPath, StatusCode::serviceUnavailable);
    EXPECT_EQ("the check result is stale", failureReason(reply, kProbeName));
}

TEST_F(PrivateHttpServerHealthTest, liveness_follows_the_application_listener)
{
    givenApplicationServer();
    whenStartPrivateServer();

    whenApplicationListenerAnswered();
    thenHealthIs(PrivateHttpServer::kLivenessPath, StatusCode::ok);

    stopApplicationServer();

    const auto reply = thenHealthIs(
        PrivateHttpServer::kLivenessPath, StatusCode::serviceUnavailable);
    EXPECT_EQ(HealthReply::Status::unavailable, reply.status);
    ASSERT_TRUE(reply.failed.has_value()) << "the reply names no failing check";
    EXPECT_TRUE(reply.failed->contains(HealthMonitor::kLivenessCheckName));
}

} // namespace nx::network::http::server::test
