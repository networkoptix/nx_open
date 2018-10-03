#include <gtest/gtest.h>

#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/http/http_client.h>
#include <nx/network/ssl/ssl_engine.h>
#include <nx/network/url/url_builder.h>

#include <mediator_service.h>

#include "mediator_functional_test.h"

namespace nx::hpm::test {

class Https:
    public MediatorFunctionalTest
{
protected:
    virtual void SetUp() override
    {
        const auto certificateFilePath =
            lm("%1/%2").args(testDataDir(), "mediator.cert").toStdString();

        ASSERT_TRUE(nx::network::ssl::Engine::useOrCreateCertificate(
            certificateFilePath.c_str(),
            "mediator/https test", "US", "Nx"));

        addArg("--https/listenOn=127.0.0.1:0");
        
        addArg("-https/certificatePath");
        addArg(certificateFilePath.c_str());

        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void assertRequestOverHttpsWorks()
    {
        auto url = baseHttpsUrl();
        url.setPath(nx::network::url::joinPath(
            nx::hpm::api::kMediatorApiPrefix,
            nx::hpm::api::kStatisticsMetricsPath).c_str());

        nx::network::http::HttpClient client;
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(nx::network::http::StatusCode::isSuccessCode(
            client.response()->statusLine.statusCode));
    }

    nx::utils::Url baseHttpsUrl() const
    {
        return nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(moduleInstance()->impl()->httpsEndpoints().front()).toUrl();
    }
};

//-------------------------------------------------------------------------------------------------

TEST_F(Https, mediator_accepts_https)
{
    assertRequestOverHttpsWorks();
}

} // namespace nx::hpm::test
