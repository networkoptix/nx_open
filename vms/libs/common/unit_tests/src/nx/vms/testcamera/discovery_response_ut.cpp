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
    QByteArray discoveryMessage;
};

TEST_F(DiscoveryResponseTest, parseWithNoLayout)
{
    const QByteArray discoveryResponseMessage = discoveryMessage + "\n;4985;92-61-00-00-00-00";
    QString outErrorMessage;
    DiscoveryResponse response(discoveryResponseMessage, &outErrorMessage);
    ASSERT_TRUE(outErrorMessage.isEmpty()) << outErrorMessage.toStdString();
}

TEST_F(DiscoveryResponseTest, parseWithLayout)
{
    const QByteArray cameraMessage("92-61-00-00-00-00/a");
    const QByteArray message = discoveryMessage + "\n;4985;" + cameraMessage;

    QString errorMessage;
    DiscoveryResponse response(message, &errorMessage);
    ASSERT_TRUE(errorMessage.isEmpty()) << errorMessage.toStdString();
    ASSERT_EQ(message, response.makeDiscoveryResponseMessage());

    errorMessage.clear();

    CameraDiscoveryResponse cameraResponse(cameraMessage, &errorMessage);
    ASSERT_TRUE(errorMessage.isEmpty()) << errorMessage.toStdString();
    ASSERT_EQ(QLatin1String("a"), cameraResponse.videoLayoutSerialized());
}

TEST_F(DiscoveryResponseTest, parseMultipleCamerasWithLayouts)
{
    const QByteArray message = discoveryMessage + "\n;4985;00-00-00-00-00-01/;00-00-00-00-00-02/b";

    QString errorMessage;
    DiscoveryResponse response(message, &errorMessage);

    ASSERT_TRUE(errorMessage.isEmpty()) << errorMessage.toStdString();
    ASSERT_EQ(4985, response.mediaPort());

    const auto responses = response.cameraDiscoveryResponses();
    ASSERT_EQ(2, responses.size());

    ASSERT_EQ(QLatin1String("00-00-00-00-00-01"), responses[0]->macAddress().toString());
    ASSERT_EQ(QLatin1String(""), responses[0]->videoLayoutSerialized());

    ASSERT_EQ(QLatin1String("00-00-00-00-00-02"), responses[1]->macAddress().toString());
    ASSERT_EQ(QLatin1String("b"), responses[1]->videoLayoutSerialized());
}

} // namespace test
} // namespace nx::vms::testcamera
