#include <gtest/gtest.h>

#include <nx/vms/testcamera/discovery_response.h>

namespace nx::vms::testcamera {
namespace test {

class DiscoveryResponseTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        discoveryMessage = QByteArray("Network Optix Camera Emulator 3.0 discovery response");
    }

    virtual void TearDown() override {}

public:
    void givenDiscoveryResponseBody(const QByteArray& body)
    {
        discoveryResponseMessage = discoveryMessage + "\n;4985;" + body;
    }

    void thenResponsesSuccessfullyParsed(size_t count)
    {
        QString outErrorMessage;
        DiscoveryResponse response(discoveryResponseMessage, &outErrorMessage);

        ASSERT_TRUE(outErrorMessage.isEmpty()) << outErrorMessage.toStdString();

        responses = response.cameraDiscoveryResponses();
        ASSERT_EQ(count, responses.size());
    }

    void thenResponseAtIndexMatches(int index, const QString& macAddress, const QString& layout)
    {
        ASSERT_EQ(macAddress, responses[index]->macAddress().toString());
        ASSERT_EQ(layout, responses[index]->videoLayoutSerialized());
    }

    QByteArray discoveryMessage;
    QByteArray discoveryResponseMessage;
    std::vector<std::shared_ptr<CameraDiscoveryResponse>> responses;
};

TEST_F(DiscoveryResponseTest, parseWithNoLayout)
{
    givenDiscoveryResponseBody("92-61-00-00-00-00");
    thenResponsesSuccessfullyParsed(1);
    thenResponseAtIndexMatches(0, "92-61-00-00-00-00", "");
}

TEST_F(DiscoveryResponseTest, parseWithLayout)
{
    givenDiscoveryResponseBody("92-61-00-00-00-00/a");
    thenResponsesSuccessfullyParsed(1);
    thenResponseAtIndexMatches(0, "92-61-00-00-00-00", "a");
}

TEST_F(DiscoveryResponseTest, parseMultipleCamerasWithLayouts)
{
    givenDiscoveryResponseBody("00-00-00-00-00-01/;00-00-00-00-00-02/b");
    thenResponsesSuccessfullyParsed(2);
    thenResponseAtIndexMatches(0, "00-00-00-00-00-01", "");
    thenResponseAtIndexMatches(1, "00-00-00-00-00-02", "b");
}

} // namespace test
} // namespace nx::vms::testcamera
