// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/network/upnp/upnp_async_client.h>

#include <gtest/gtest.h>

#include <nx/utils/std/future.h>

namespace nx::network::upnp::test {

using namespace nx::network::http;

static const nx::Url URL(lit("http://192.168.20.1:52869/ctl/IPConn"));
static const auto TCP = AsyncClient::Protocol::tcp;
static constexpr char INTERNAL_IP[] = "192.168.20.77";
static constexpr char EXTERNAL_IP[] = "10.0.2.161";
static constexpr char DESC[] = "test";

TEST(UpnpAsyncClient, DISABLED_Cancel)
{
    for (size_t testNumber = 0; testNumber <= 5; ++testNumber)
    {
        AsyncClient client;
        client.externalIp(URL, [&](const HostAddress&) {});
        if (testNumber)
        {
            std::chrono::milliseconds delay(testNumber * testNumber * 200);
            std::this_thread::sleep_for(delay);
        }
    }
}

// TODO: implement over test HTTP server
TEST(UpnpAsyncClient, DISABLED_GetIp)
{
    AsyncClient client;
    {
        nx::utils::promise< HostAddress > prom;
        client.externalIp(URL,
            [&](const HostAddress& v) { prom.set_value(v); });
        EXPECT_EQ(prom.get_future().get().toString(), EXTERNAL_IP);
    }
    {
        nx::utils::promise< AsyncClient::MappingInfoResult > prom;
        client.getMapping(URL, 8877, TCP,
            [&](AsyncClient::MappingInfoResult result)
        { prom.set_value(result); });

        // no such mapping
        EXPECT_FALSE(prom.get_future().get().has_value());
    }
    {
        nx::utils::promise< bool > prom;
        client.deleteMapping(URL, 8877, TCP,
            [&](bool v) { prom.set_value(v); });
        EXPECT_FALSE(prom.get_future().get()); // no such mapping
    }
    {
        nx::utils::promise< AsyncClient::MappingInfoList > prom;
        client.getAllMappings(URL,
            [&](AsyncClient::MappingInfoList mappings, bool) { prom.set_value(mappings); });
        EXPECT_EQ(prom.get_future().get().size(), 2U); // mappings from Skype ;)
    }
}

// TODO: implement over test HTTP server
TEST(UpnpAsyncClient, DISABLED_Mapping)
{
    AsyncClient client;
    {
        nx::utils::promise< bool > prom;
        client.addMapping(URL, INTERNAL_IP, 80, 80, TCP, DESC, 0,
            [&](bool v) { prom.set_value(v); });

        EXPECT_FALSE(prom.get_future().get()); // wrong port
    }
    {
        nx::utils::promise< bool > prom;
        client.addMapping(URL, INTERNAL_IP, 80, 8877, TCP, DESC, 0,
            [&](bool v) { prom.set_value(v); });

        ASSERT_TRUE(prom.get_future().get());
    }
    {
        nx::utils::promise< SocketAddress > prom;
        client.getMapping(URL, 8877, TCP,
            [&](AsyncClient::MappingInfoResult result)
            { prom.set_value(SocketAddress(result->internalIp, result->internalPort)); });

        EXPECT_EQ(prom.get_future().get().toString(),
            SocketAddress(INTERNAL_IP, 80).toString());
    }
    {
        nx::utils::promise< bool > prom;
        client.deleteMapping(URL, 8877, TCP,
            [&](bool v) { prom.set_value(v); });

        EXPECT_TRUE(prom.get_future().get());
    }
}

struct Params
{
    // Input.
    StatusCode::Value statusCode;
    QByteArray response;

    // Output.
    bool expectedHasValue = false;
    AsyncClient::Message expectedMessage;
};

void PrintTo(const Params& params, std::ostream* os)
{
    *os << "\nCode: " << std::to_string(params.statusCode)
        << "\nResponse: " << params.response.toStdString()
        << "\nHasValue: " << (params.expectedHasValue ? "y" : "n")
        << "\nMessage: " << params.expectedMessage.toString().toStdString();
}

static const QByteArray errorResponse = R"(
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
    <s:Body>
    <s:Fault>
        <faultCode>s:Client</faultCode>
        <faultString>UPnPError</faultString>
        <detail>
        <UPnPError xmlns="urn:schemas-upnp-org:control-1-0">
            <errorCode>402</errorCode>
            <errorDescription>Invalid Args</errorDescription>
        </UPnPError>
        </detail>
    </s:Fault>
    </s:Body>
</s:Envelope>)";

static const QByteArray mappingEntryResponse = R"(
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
  <s:Body>
    <u:GetGenericPortMappingEntryResponse xmlns:u="urn:schemas-upnp-org:service:WANIPConnection:1">
      <NewRemoteHost></NewRemoteHost>
      <NewExternalPort>31337</NewExternalPort>
      <NewProtocol>TCP</NewProtocol>
      <NewInternalPort>2</NewInternalPort>
      <NewInternalClient>4.3.2.1</NewInternalClient>
      <NewEnabled>1</NewEnabled>
      <NewPortMappingDescription>Test</NewPortMappingDescription>
      <NewLeaseDuration>10000</NewLeaseDuration>
    </u:GetGenericPortMappingEntryResponse>
  </s:Body>
</s:Envelope>)";

static const QByteArray genericSuccessResponse = R"(
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
  <s:Body>
    <%2:%1Response xmlns:%3="urn:schemas-upnp-org:service:WANIPConnection:1"></%2:%1Response>
  </s:Body>
</s:Envelope>)";

static const QByteArray externalIpAddressResponse = R"(
<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
  <s:Body>
    <u:GetExternalIPAddressResponse xmlns:u="urn:schemas-upnp-org:service:WANIPConnection:1">
      <NewExternalIPAddress>1.2.3.4</NewExternalIPAddress>
    </u:GetExternalIPAddressResponse>
  </s:Body>
</s:Envelope>)";

class ResponseParserFixture: public ::testing::TestWithParam<Params>
{
public:
    static std::string PrintToString(const testing::TestParamInfo<Params>& info)
    {
        const auto& param = info.param;
        const QString name = param.expectedMessage.isOk()
            ? param.expectedMessage.action
            : ("Error" + param.expectedMessage.getParam("errorCode"));
        return std::to_string(param.statusCode) + "_" + name.toStdString();
    }
};

INSTANTIATE_TEST_SUITE_P(
    UpnpAsyncClient,
    ResponseParserFixture,
    ::testing::Values(
        // Empty, successful code.
        Params{
            .statusCode = StatusCode::ok,
            .expectedHasValue = false,
        },
        // Empty, unsuccessful code.
        Params{
            .statusCode = StatusCode::badRequest,
            .expectedHasValue = false,
        },
        // Error 402/713/725/etc. - they all the same except the code and description.
        Params{
            .statusCode = StatusCode::internalServerError,
            .response = errorResponse,
            .expectedHasValue = true,
            .expectedMessage = AsyncClient::Message(QString(), QString(),
                {
                    {"faultCode", "s:Client"},
                    {"faultString", "UPnPError"},
                    {"errorCode", "402"},
                    {"errorDescription", "Invalid Args"}
                })
        },
        // GetGenericPortMappingEntry response with active mapping information.
        Params{
            .statusCode = StatusCode::ok,
            .response = mappingEntryResponse,
            .expectedHasValue = true,
            .expectedMessage = AsyncClient::Message(
                "GetGenericPortMappingEntry",
                AsyncClient::kWanIp,
                {
                    {"NewRemoteHost", ""},
                    {"NewExternalPort", "31337"},
                    {"NewProtocol", "TCP"},
                    {"NewInternalPort", "2"},
                    {"NewInternalClient", "4.3.2.1"},
                    {"NewEnabled", "1"},
                    {"NewPortMappingDescription", "Test"},
                    {"NewLeaseDuration", "10000"},
                })
        },
        // Successful AddPortMapping response.
        Params{
            .statusCode = StatusCode::ok,
            .response = nx::format(genericSuccessResponse, "AddPortMapping", "u", "u").toUtf8(),
            .expectedHasValue = true,
            .expectedMessage = AsyncClient::Message(
                "AddPortMapping",
                AsyncClient::kWanIp,
                {
                })
        },
        // Successful GetExternalIPAddress response.
        Params{
            .statusCode = StatusCode::ok,
            .response = externalIpAddressResponse,
            .expectedHasValue = true,
            .expectedMessage = AsyncClient::Message(
                "GetExternalIPAddress",
                AsyncClient::kWanIp,
                {
                    {"NewExternalIPAddress", "1.2.3.4"}
                })
        },
        // Successful response with namespace named other than usual "u".
        Params{
            .statusCode = StatusCode::ok,
            .response = nx::format(genericSuccessResponse, "DifferentNS", "m", "m").toUtf8(),
            .expectedHasValue = true,
            .expectedMessage = AsyncClient::Message(
                "DifferentNS",
                AsyncClient::kWanIp,
                {
                })
        },
        // Successful response with broken namespace.
        // See comment for `knownBrokenNamespaces()` function.
        Params{
            .statusCode = StatusCode::ok,
            .response = nx::format(genericSuccessResponse, "Broken", "u", "u0").toUtf8(),
            .expectedHasValue = true,
            .expectedMessage = AsyncClient::Message(
                "Broken",
                AsyncClient::kWanIp,
                {
                })
        }
    ),
    ResponseParserFixture::PrintToString
);

TEST_P(ResponseParserFixture, Parse)
{
    const auto& param = GetParam();
    const auto result = AsyncClient::parseResponse(param.statusCode, param.response);
    ASSERT_EQ(result.has_value(), param.expectedHasValue);
    if (!result)
        return;

    ASSERT_EQ(result->isOk(), param.expectedMessage.isOk());
    ASSERT_EQ(result->action, param.expectedMessage.action);
    ASSERT_EQ(result->service, param.expectedMessage.service);
    ASSERT_EQ(result->params.size(), param.expectedMessage.params.size());
    for (const auto& [key, value]: param.expectedMessage.params)
    {
        const auto it = result->params.find(key);
        ASSERT_NE(it, result->params.end());
        ASSERT_EQ(it->second, value);
    }
}

} // namespace nx::network::upnp::test
