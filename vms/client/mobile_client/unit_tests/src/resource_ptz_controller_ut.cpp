// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/ptz/basic_ptz_controller.h>
#include <nx/mobile_client/controllers/resource_ptz_controller.h>

namespace nx::client::mobile::test {

namespace {

class ViewportController: public QnBasicPtzController
{
public:
    ViewportController(): QnBasicPtzController({}) {}

    virtual bool viewportMove(
        qreal aspectRatio, const QRectF& viewport, const Options& options) override
    {
        receivedAspectRatio = aspectRatio;
        receivedViewport = viewport;
        receivedOptions = options;
        return result;
    }

    qreal receivedAspectRatio = 0;
    QRectF receivedViewport;
    std::optional<Options> receivedOptions;
    bool result = true;
};

} // namespace

TEST(ResourcePtzController, numericViewportInvocationForwardsSpeed)
{
    const auto backend = QSharedPointer<ViewportController>::create();
    ResourcePtzController controller;
    controller.setBaseController(backend);
    const QRectF viewport(0.1, 0.2, 0.5, 0.5);

    for (const auto speed: {0.0, 0.25, 1.0})
    {
        for (const auto result: {false, true})
        {
            backend->receivedOptions.reset();
            backend->result = result;
            bool returned = !result;
            ASSERT_TRUE(QMetaObject::invokeMethod(&controller,
                "viewportMove",
                Qt::DirectConnection,
                Q_RETURN_ARG(bool, returned),
                Q_ARG(qreal, 1.5),
                Q_ARG(QRectF, viewport),
                Q_ARG(qreal, speed)));
            EXPECT_EQ(returned, result);
            ASSERT_TRUE(backend->receivedOptions);
            EXPECT_EQ(backend->receivedOptions->type, nx::vms::common::ptz::Type::operational);
            EXPECT_EQ(backend->receivedOptions->speed, speed);
            EXPECT_EQ(backend->receivedAspectRatio, 1.5);
            EXPECT_EQ(backend->receivedViewport, viewport);
        }
    }
}

} // namespace nx::client::mobile::test
