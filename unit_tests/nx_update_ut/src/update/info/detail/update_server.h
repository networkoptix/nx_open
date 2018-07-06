#pragma once

#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <rest/server/json_rest_handler.h>
#include <nx/update/info/detail/data_provider/test_support/json_data.h>

namespace nx {
namespace update {
namespace info {
namespace test {
namespace detail {

class UpdateServer
{
public:
    UpdateServer()
    {
        registerHandlers();
        NX_ASSERT(start());
    }

    network::SocketAddress address() const
    {
        return m_httpServer.serverAddress();
    }

private:
    const QString m_updatesPath = "/updates.json";
    const QString m_updatePathSuffix = "/update.json";
    const QByteArray m_jsonMimeType = "application/json";

    network::http::TestHttpServer m_httpServer;

    void registerHandlers()
    {
        using namespace info::detail::data_provider;
        m_httpServer.registerStaticProcessor(
            m_updatesPath,
            test_support::metaDataJson(),
            m_jsonMimeType);

        for (const auto& updateTestData : test_support::updateTestDataList())
        {
            QString updatePath = lit("/%1/%2%3")
                .arg(updateTestData.customization)
                .arg(updateTestData.version)
                .arg(m_updatePathSuffix);
            m_httpServer.registerStaticProcessor(updatePath, updateTestData.json, m_jsonMimeType);
        }
    }

    bool start()
    {
        return m_httpServer.bindAndListen();
    }
};

} // namespace test
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx