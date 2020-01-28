#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include <media_server/media_server_module.h>

#include <nx/vms/server/analytics/object_coordinates_translator.h>
#include <nx/mediaserver/camera_mock.h>

namespace nx::vms::server::analytics {

using CameraMock = nx::vms::server::resource::test::CameraMock;

static const QRectF kRectangle(0.2, 0.3, 0.25, 0.15);

static const std::map<int, QRectF> kTranslatedRectanglesByRotationAngle = {
    {0, kRectangle},
    {90, QRectF(0.3, 0.55, 0.15, 0.25)},
    {180, QRectF(0.55, 0.55, 0.25, 0.15)},
    {270, QRectF(0.55, 0.2, 0.15, 0.25)},
};

class ObjectCoordinatesTranslatorTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_serverModule = std::make_unique<QnMediaServerModule>();
        m_device = QnVirtualCameraResourcePtr(new CameraMock(m_serverModule.get()));
        m_translator = std::make_unique<ObjectCoordinatesTranslator>(m_device);
    }

protected:
    void updateDeviceRotationTo(int rotationAngleDegrees)
    {
        m_device->setProperty(
            QnMediaResource::rotationKey(), QString::number(rotationAngleDegrees));
    }

    void givenDeviceWithVideoRotatedTo(int rotationAngleDegrees)
    {
        updateDeviceRotationTo(rotationAngleDegrees);
    }

    void translateRectangleForThisDevice()
    {
        m_translatedRectangle = m_translator->translate(kRectangle);
    }

    void verifyThatRectanlgeHasBeenTranslatedCorrectlyFrom(int rotationAngleDegrees)
    {
        const std::optional<QRectF> correctTranslatedRectangle =
            getCorrectTranslatedRectangle(rotationAngleDegrees);

        ASSERT_TRUE(correctTranslatedRectangle.has_value());
        ASSERT_EQ(m_translatedRectangle, *correctTranslatedRectangle);
    }

    void verifyThatRectanlgeHasBeenTranslatedCorrectly()
    {
        const int rotationAngleDegrees =
            m_device->getProperty(QnMediaResource::rotationKey()).toInt();

        verifyThatRectanlgeHasBeenTranslatedCorrectlyFrom(rotationAngleDegrees);
    }

    void verifyThatResultingRectangleIsNull()
    {
        ASSERT_TRUE(m_translatedRectangle.isNull());
    }

private:
    std::optional<QRectF> getCorrectTranslatedRectangle(int rotationAngleDegrees)
    {
        const auto it = kTranslatedRectanglesByRotationAngle.find(rotationAngleDegrees);
        if (it == kTranslatedRectanglesByRotationAngle.cend())
            return std::nullopt;

        return it->second;
    }

private:
    QRectF m_translatedRectangle;
    std::unique_ptr<QnMediaServerModule> m_serverModule;
    QnVirtualCameraResourcePtr m_device;
    std::unique_ptr<ObjectCoordinatesTranslator> m_translator;
};

TEST_F(ObjectCoordinatesTranslatorTest, verify0DegreesRotationTranslation)
{
    givenDeviceWithVideoRotatedTo(0);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectly();
}

TEST_F(ObjectCoordinatesTranslatorTest, verify90DegreesRotationTranslation)
{
    givenDeviceWithVideoRotatedTo(90);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectly();
}

TEST_F(ObjectCoordinatesTranslatorTest, verify180DegreesRotationTranslation)
{
    givenDeviceWithVideoRotatedTo(180);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectly();
}

TEST_F(ObjectCoordinatesTranslatorTest, verify270DegreesRotationTranslation)
{
    givenDeviceWithVideoRotatedTo(270);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectly();
}

TEST_F(ObjectCoordinatesTranslatorTest, verifyThatIncorrectTranslationAngleGivesNullRectangle)
{
    givenDeviceWithVideoRotatedTo(235);
    translateRectangleForThisDevice();
    verifyThatResultingRectangleIsNull();
}

TEST_F(ObjectCoordinatesTranslatorTest, verifyThatChangingDeviceRotationPropertyAffectsTranslation)
{
    givenDeviceWithVideoRotatedTo(0);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectlyFrom(0);

    updateDeviceRotationTo(90);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectlyFrom(90);

    updateDeviceRotationTo(180);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectlyFrom(180);

    updateDeviceRotationTo(270);
    translateRectangleForThisDevice();
    verifyThatRectanlgeHasBeenTranslatedCorrectlyFrom(270);
}

} // namespace nx::vms::server::analytics
