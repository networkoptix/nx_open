// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <optional>

#include <core/ptz/basic_ptz_controller.h>
#include <core/ptz/fallback_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <nx/vms/client/desktop/test_support/client_camera_resource_stub.h>
#include <ui/fisheye/fisheye_ptz_controller.h>

namespace {

using namespace nx::vms::common::ptz;
using nx::vms::client::desktop::test::ClientCameraResourceStub;

class ModePtzController: public QnBasicPtzController
{
public:
    ModePtzController(const QnResourcePtr& resource, Types types):
        QnBasicPtzController(resource),
        m_types(types)
    {
    }

    virtual Types getSupportedPtzTypes() const override { return m_types; }

    void setSupportedTypes(Types types)
    {
        m_types = types;
        emit changed(DataField::capabilities);
    }

    void setCapableTypes(Types types)
    {
        m_capableTypes = types;
        emit changed(DataField::capabilities);
    }

    virtual bool continuousMove(const Vector&, const Options& options) override
    {
        lastMoveType = options.type;
        return true;
    }

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override
    {
        return supportsPtzType(options.type) && m_capableTypes.testFlag(options.type)
            ? Ptz::Capability::continuousPanTiltZoom
            : Ptz::Capability::none;
    }

    std::optional<Type> lastMoveType;

private:
    Types m_types;
    Types m_capableTypes = Type::any;
};

/** Sends a move and returns the only controller that received it with the given type. */
const ModePtzController* moveTarget(QnFallbackPtzController& controller,
    ModePtzController& main,
    ModePtzController& fallback,
    Type type)
{
    main.lastMoveType.reset();
    fallback.lastMoveType.reset();
    if (!controller.continuousMove({}, {type}))
        return nullptr;

    if (main.lastMoveType == type && !fallback.lastMoveType)
        return &main;
    if (fallback.lastMoveType == type && !main.lastMoveType)
        return &fallback;
    return nullptr;
}

/** Fisheye controller wrapped as in the rules PTZ preset picker. */
QnPtzControllerPtr makePickerFisheyeController(const QnVirtualCameraResourcePtr& camera)
{
    return QnPtzControllerPtr(
        new QnPresetPtzController(QnPtzControllerPtr(new QnFisheyePtzController(camera))));
}

TEST(FallbackPtzController, reportsTypesFromBothControllers)
{
    const QnResourcePtr resource(new QnResource());
    for (const auto mainType: kAllTypes)
    {
        const auto fallbackType =
            mainType == Type::operational ? Type::configurational : Type::operational;
        const QnPtzControllerPtr main(new ModePtzController(resource, mainType));
        const QnPtzControllerPtr fallback(new ModePtzController(resource, fallbackType));
        const QnFallbackPtzController controller(main, fallback);
        EXPECT_EQ(controller.getSupportedPtzTypes(), Types(Type::any));
        EXPECT_TRUE(controller.supportsPtzType(mainType));
        EXPECT_TRUE(controller.supportsPtzType(fallbackType));
        EXPECT_FALSE(controller.supportsPtzType(Type::any));
    }
}

TEST(FallbackPtzController, preservesSingleModeAndEmptyDeclarations)
{
    const QnResourcePtr resource(new QnResource());
    const QnPtzControllerPtr empty(new ModePtzController(resource, {}));
    const QnPtzControllerPtr operational(new ModePtzController(resource, Type::operational));
    EXPECT_EQ(QnFallbackPtzController(empty, operational).getSupportedPtzTypes(),
        Types(Type::operational));
    EXPECT_EQ(QnFallbackPtzController(operational, empty).getSupportedPtzTypes(),
        Types(Type::operational));
    EXPECT_EQ(QnFallbackPtzController(empty, empty).getSupportedPtzTypes(), Types{});
}

TEST(FallbackPtzController, selectsMainForAnyAvailableRequestedTypeAndRefreshes)
{
    const QnResourcePtr resource(new QnResource());
    const QSharedPointer<ModePtzController> main(new ModePtzController(resource, {}));
    const QSharedPointer<ModePtzController> fallback(new ModePtzController(resource, Type::any));
    QnFallbackPtzController controller(main, fallback);

    for (const Types mainTypes:
        {Types{}, Types(Type::operational), Types(Type::configurational), Types(Type::any)})
    {
        main->setSupportedTypes(mainTypes);
        for (const auto requestedType: {Type::operational, Type::configurational, Type::any})
        {
            main->lastMoveType.reset();
            fallback->lastMoveType.reset();
            ASSERT_TRUE(controller.continuousMove({}, {requestedType}));
            const bool useMain = (mainTypes & requestedType) != Types{};
            EXPECT_EQ(main->lastMoveType.has_value(), useMain);
            EXPECT_EQ(fallback->lastMoveType.has_value(), !useMain);
            EXPECT_EQ((useMain ? main : fallback)->lastMoveType, requestedType);
        }
    }

    main->setSupportedTypes({});
    main->lastMoveType.reset();
    fallback->lastMoveType.reset();
    ASSERT_TRUE(controller.continuousMove({}, {Type::any}));
    EXPECT_FALSE(main->lastMoveType.has_value());
    EXPECT_EQ(fallback->lastMoveType, Type::any);
}

TEST(FallbackPtzController, followsMainCapabilityChanges)
{
    const QnResourcePtr resource(new QnResource());
    const QSharedPointer<ModePtzController> main(
        new ModePtzController(resource, Type::operational));
    const QSharedPointer<ModePtzController> fallback(
        new ModePtzController(resource, Type::operational));
    QnFallbackPtzController controller(main, fallback);
    EXPECT_EQ(moveTarget(controller, *main, *fallback, Type::operational), main.get());

    std::optional<Ptz::Capabilities> capabilitiesOnChange;
    QObject::connect(&controller,
        &QnAbstractPtzController::changed,
        [&](DataFields fields)
        {
            if (fields.testFlag(DataField::capabilities))
                capabilitiesOnChange = controller.getCapabilities({Type::operational});
        });

    main->setCapableTypes({});
    EXPECT_EQ(capabilitiesOnChange, Ptz::Capabilities(Ptz::Capability::continuousPanTiltZoom));
    EXPECT_EQ(moveTarget(controller, *main, *fallback, Type::operational), fallback.get());
    EXPECT_EQ(controller.getCapabilities({Type::operational}),
        Ptz::Capabilities(Ptz::Capability::continuousPanTiltZoom));

    main->setCapableTypes(Type::operational);
    EXPECT_EQ(moveTarget(controller, *main, *fallback, Type::operational), main.get());
}

TEST(FallbackPtzController, routesByCapabilitiesPerType)
{
    const QnResourcePtr resource(new QnResource());
    const QSharedPointer<ModePtzController> main(new ModePtzController(resource, Type::any));
    const QSharedPointer<ModePtzController> fallback(new ModePtzController(resource, Type::any));
    main->setCapableTypes(Type::configurational);
    QnFallbackPtzController controller(main, fallback);

    EXPECT_EQ(moveTarget(controller, *main, *fallback, Type::operational), fallback.get());
    EXPECT_EQ(moveTarget(controller, *main, *fallback, Type::configurational), main.get());
    EXPECT_EQ(moveTarget(controller, *main, *fallback, Type::any), main.get());
}

TEST(FallbackPtzController, reportsTypesSupportedByRoutedController)
{
    const QnResourcePtr resource(new QnResource());
    const QSharedPointer<ModePtzController> main(
        new ModePtzController(resource, Type::operational));
    const QSharedPointer<ModePtzController> fallback(
        new ModePtzController(resource, Type::configurational));
    QnFallbackPtzController controller(main, fallback);
    EXPECT_EQ(controller.getSupportedPtzTypes(), Types(Type::any));

    main->setCapableTypes({});
    EXPECT_EQ(controller.getSupportedPtzTypes(), Types(Type::configurational));
    EXPECT_FALSE(controller.supportsPtzType(Type::operational));
}

TEST(FallbackPtzController, routesFisheyeWithoutDewarpingToFallback)
{
    const QnVirtualCameraResourcePtr camera(new ClientCameraResourceStub());
    const QSharedPointer<ModePtzController> server(
        new ModePtzController(camera, Type::operational));
    QnFallbackPtzController controller(makePickerFisheyeController(camera), server);

    EXPECT_EQ(controller.getCapabilities({Type::operational}),
        Ptz::Capabilities(Ptz::Capability::continuousPanTiltZoom));
    ASSERT_TRUE(controller.continuousMove({}, {Type::operational}));
    EXPECT_EQ(server->lastMoveType, Type::operational);
}

TEST(FallbackPtzController, routesFisheyeWithDewarpingToFisheye)
{
    const QnVirtualCameraResourcePtr camera(new ClientCameraResourceStub());
    nx::vms::api::dewarping::MediaData dewarping;
    dewarping.enabled = true;
    camera->setDewarpingParams(dewarping);
    const QSharedPointer<ModePtzController> server(
        new ModePtzController(camera, Type::operational));
    QnFallbackPtzController controller(makePickerFisheyeController(camera), server);

    EXPECT_TRUE(
        controller.getCapabilities({Type::operational}).testFlag(Ptz::Capability::virtual_));
    ASSERT_TRUE(controller.continuousMove({}, {Type::operational}));
    EXPECT_FALSE(server->lastMoveType.has_value());
}

} // namespace
