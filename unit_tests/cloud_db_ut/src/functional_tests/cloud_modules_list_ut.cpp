#include <gtest/gtest.h>

#include <fstream>
#include <list>

#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/cdb/client/cdb_request_path.h>
#include <nx/cloud/cdb/managers/cloud_module_url_provider.h>

#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

static const char xmlTemplate[] = R"xml(
<?xml version="1.0" encoding="utf-8"?>
<sequence>
    <set resName="cdb" resValue="https://%host%:443"/>
    <set resName="hpm" resValue="stun://%host%:3345"/>
    <set resName="notification_module" resValue="https://%host%:443"/>
</sequence>
)xml";

static const char hostName[] = "cloud-dev.hdw.mx";

static const char resultingXml[] = R"xml(
<?xml version="1.0" encoding="utf-8"?>
<sequence>
    <set resName="cdb" resValue="https://cloud-dev.hdw.mx:443"/>
    <set resName="hpm" resValue="stun://cloud-dev.hdw.mx:3345"/>
    <set resName="notification_module" resValue="https://cloud-dev.hdw.mx:443"/>
</sequence>
)xml";

class CloudModuleUrlProvider:
    public ::testing::Test,
    public utils::test::TestWithTemporaryDirectory
{
public:
    CloudModuleUrlProvider():
        utils::test::TestWithTemporaryDirectory("cdb", QString())
    {
    }

protected:
    void givenLoadedCloudModulesXmlTemplate()
    {
        const auto templateFilePath = createTemporaryXmlTemplateFile();
        m_moduleUrlProvider = 
            std::make_unique<cdb::CloudModuleUrlProvider>(templateFilePath);
    }

    void tryToLoadInvalidFile()
    {
        m_moduleUrlProvider = 
            std::make_unique<cdb::CloudModuleUrlProvider>(
                lm("/%1").arg(QnUuid::createUuid().toString()));
    }

    void whenCloudModulesXmlHasBeenRequested()
    {
        m_resultingXml = m_moduleUrlProvider->getCloudModulesXml(hostName);
    }

    void assertHostHasBeenCorrectlyInserted()
    {
        ASSERT_EQ(resultingXml, m_resultingXml);
    }

private:
    std::unique_ptr<cdb::CloudModuleUrlProvider> m_moduleUrlProvider;
    QByteArray m_resultingXml;

    QString createTemporaryXmlTemplateFile()
    {
        const QString tmpFilePath = lm("%1/cloud_modules_template.xml").arg(testDataDir());

        QFile templateFile(tmpFilePath);
        NX_GTEST_ASSERT_TRUE(templateFile.open(QIODevice::WriteOnly));
        templateFile.write(xmlTemplate);

        return tmpFilePath;
    }
};

TEST_F(CloudModuleUrlProvider, host_inserted_correctly)
{
    givenLoadedCloudModulesXmlTemplate();
    whenCloudModulesXmlHasBeenRequested();
    assertHostHasBeenCorrectlyInserted();
}

TEST_F(CloudModuleUrlProvider, not_found_template_file_causes_error)
{
    ASSERT_ANY_THROW(tryToLoadInvalidFile());
}

//-------------------------------------------------------------------------------------------------
// FtCloudModulesXml

class FtCloudModulesXml:
    public CdbFunctionalTest
{
public:
    FtCloudModulesXml():
        m_expectedHost("123.example.com")
    {
        init();
    }

protected:
    void useFullHttpHostHeader()
    {
        m_additionalHttpHeaders.emplace_back("Host", m_expectedHost.toUtf8() + ":80");
    }

    void whenRequestedSomeModuleUrl()
    {
        whenRequestedModuleUrlWithFetcher<network::cloud::CloudDbUrlFetcher>();
    }

    template<typename FetcherType>
    void whenRequestedModuleUrlWithFetcher()
    {
        using namespace std::placeholders;

        network::SocketGlobals::addressResolver().addFixedAddress(
            m_expectedHost, endpoint());

        FetcherType fetcher;
        auto fetcherGuard = makeScopeGuard([&fetcher]() { fetcher.pleaseStopSync(); });

        fetcher.setModulesXmlUrl(QUrl(lm("http://%1:%2%3")
            .arg(m_expectedHost).arg(endpoint().port).arg(kDeprecatedCloudModuleXmlPath)));
        for (const auto& header: m_additionalHttpHeaders)
            fetcher.addAdditionalHttpHeaderForGetRequest(header.first, header.second);
        nx::utils::promise<void> done;
        auto completionHandler = 
            [this, &done](nx_http::StatusCode::Value statusCode, QUrl moduleUrl)
            {
                m_fetchUrlResult = {statusCode, moduleUrl};
                done.set_value();
            };
        fetcher.get(std::bind(completionHandler, _1, _2));

        done.get_future().wait();

        network::SocketGlobals::addressResolver().removeFixedAddress(
            m_expectedHost, endpoint());
    }
    
    void assertHostHasBeenTakenFromHttpRequest()
    {
        ASSERT_EQ(nx_http::StatusCode::ok, m_fetchUrlResult.first);
        ASSERT_EQ(m_expectedHost, m_fetchUrlResult.second.host());
    }

    void assertModuleUrlHasBeenSuccessfullyReported()
    {
        ASSERT_EQ(nx_http::StatusCode::ok, m_fetchUrlResult.first);
        ASSERT_FALSE(m_fetchUrlResult.second.host().isEmpty());
        ASSERT_FALSE(m_fetchUrlResult.second.scheme().isEmpty());
    }

private:
    QString m_expectedHost;
    std::pair<nx_http::StatusCode::Value, QUrl> m_fetchUrlResult;
    std::list<std::pair<nx::String, nx::String>> m_additionalHttpHeaders;

    void init()
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }
};

TEST_F(FtCloudModulesXml, host_is_taken_from_http_request)
{
    whenRequestedSomeModuleUrl();
    assertHostHasBeenTakenFromHttpRequest();
}

TEST_F(FtCloudModulesXml, host_header_contains_port)
{
    useFullHttpHostHeader();
    whenRequestedSomeModuleUrl();
    assertHostHasBeenTakenFromHttpRequest();
}

TEST_F(FtCloudModulesXml, default_xml_is_compatible_with_url_fetcher)
{
    whenRequestedSomeModuleUrl();
    assertModuleUrlHasBeenSuccessfullyReported();
}

TEST_F(FtCloudModulesXml, default_xml_contains_mediator_url)
{
    whenRequestedModuleUrlWithFetcher<network::cloud::ConnectionMediatorUrlFetcher>();
    assertModuleUrlHasBeenSuccessfullyReported();
}

TEST_F(FtCloudModulesXml, default_xml_contains_cloud_db_url)
{
    whenRequestedModuleUrlWithFetcher<network::cloud::CloudDbUrlFetcher>();
    assertModuleUrlHasBeenSuccessfullyReported();
}

} // namespace test
} // namespace cdb
} // namespace nx
