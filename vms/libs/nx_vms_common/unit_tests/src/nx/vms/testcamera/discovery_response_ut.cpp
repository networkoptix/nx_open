// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/kit/utils.h>
#include <nx/vms/testcamera/discovery_response.h>

namespace nx::vms::testcamera {
namespace test {

class DiscoveryResponseTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        discoveryMessage = QByteArray("Nx Testcamera discovery response");
    }

    virtual void TearDown() override {}

public:
    void givenDiscoveryResponseBody(const QByteArray& body)
    {
        discoveryResponseMessage = discoveryMessage + "\n;4985;" + body;
    }

    void thenResponsesParsedWithError()
    {
        QString outErrorMessage;
        DiscoveryResponse response(discoveryResponseMessage, &outErrorMessage);
        ASSERT_FALSE(outErrorMessage.isEmpty());
    }

    void thenResponsesSuccessfullyParsed(size_t count)
    {
        QString outErrorMessage;
        DiscoveryResponse response(discoveryResponseMessage, &outErrorMessage);

        ASSERT_TRUE(outErrorMessage.isEmpty()) << outErrorMessage.toStdString();
        ASSERT_EQ(4985, response.mediaPort());

        responses = response.cameraDiscoveryResponses();
        ASSERT_EQ(count, responses.size());
    }

    void thenResponseAtIndexMatches(
        int index,
        const QString& macAddress,
        const QString& layout, const QJsonObject& payload)
    {
        ASSERT_EQ(macAddress, responses[index]->macAddress().toString());
        ASSERT_EQ(layout, responses[index]->videoLayoutSerialized());
        ASSERT_EQ(payload, responses[index]->payload());
    }

    QByteArray discoveryMessage;
    QByteArray discoveryResponseMessage;
    std::vector<std::shared_ptr<CameraDiscoveryResponse>> responses;
};

TEST_F(DiscoveryResponseTest, parseWithNoLayout)
{
    givenDiscoveryResponseBody("92-61-00-00-00-00");
    thenResponsesSuccessfullyParsed(1);
    thenResponseAtIndexMatches(0, "92-61-00-00-00-00", "", {});
}

TEST_F(DiscoveryResponseTest, parseWithLayout)
{
    givenDiscoveryResponseBody("92-61-00-00-00-00/width=2&height=1");
    thenResponsesSuccessfullyParsed(1);
    thenResponseAtIndexMatches(0, "92-61-00-00-00-00", "width=2;height=1", {});
}

TEST_F(DiscoveryResponseTest, parseMultipleCamerasWithLayouts)
{
    givenDiscoveryResponseBody("00-00-00-00-00-01/;00-00-00-00-00-02/b");
    thenResponsesSuccessfullyParsed(2);
    thenResponseAtIndexMatches(0, "00-00-00-00-00-01", "", {});
    thenResponseAtIndexMatches(1, "00-00-00-00-00-02", "b", {});
}

TEST_F(DiscoveryResponseTest, parseWithPayload)
{
    givenDiscoveryResponseBody(R"(92-61-00-00-00-00/layout/{"key":"value%3B%2F%25\n"})");
    thenResponsesSuccessfullyParsed(1);
    thenResponseAtIndexMatches(0, "92-61-00-00-00-00", "layout", {{"key", "value;/%\n"}});
}

TEST_F(DiscoveryResponseTest, parseWithPayloadAndEmptyLayout)
{
    givenDiscoveryResponseBody("92-61-00-00-00-00//{}");
    thenResponsesSuccessfullyParsed(1);
    thenResponseAtIndexMatches(0, "92-61-00-00-00-00", "", {});
}

TEST_F(DiscoveryResponseTest, serializeWithPayloadOnly)
{
    CameraDiscoveryResponse cameraResponse(
        nx::utils::MacAddress(QLatin1String("92-61-00-00-00-00")),
        QByteArray(),
        QJsonObject({{"%;/ \nv", ""}}));

    ASSERT_EQ(
        QByteArray(R"(92-61-00-00-00-00//{"%25%3B%2F \nv":""})"),
        cameraResponse.makeCameraDiscoveryResponseMessagePart());
}

TEST_F(DiscoveryResponseTest, parseEmptyBodyError)
{
    givenDiscoveryResponseBody("");
    thenResponsesParsedWithError();
}

TEST_F(DiscoveryResponseTest, parseInvalidMacAddressError)
{
    givenDiscoveryResponseBody("92-61-00:00-00-00");
    thenResponsesParsedWithError();
}

TEST_F(DiscoveryResponseTest, parseSecondResponseTooManySlashesError)
{
    givenDiscoveryResponseBody("00-00-00-00-00-01/;00-00-00-00-00-02///");
    thenResponsesParsedWithError();
}

TEST_F(DiscoveryResponseTest, parseUnescapedSemicolonError)
{
    givenDiscoveryResponseBody(R"(92-61-00-00-00-00/layout/{"key":"value;\n"})");
    thenResponsesParsedWithError();
}

} // namespace test
} // namespace nx::vms::testcamera
