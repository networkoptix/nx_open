// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <optional>

#include <core/ptz/basic_ptz_controller.h>
#include <core/ptz/fallback_ptz_controller.h>
#include <core/resource/resource.h>

namespace {

using namespace nx::vms::common::ptz;

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

    virtual bool continuousMove(const Vector&, const Options& options) override
    {
        lastMoveType = options.type;
        return true;
    }

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override
    {
        return supportsPtzType(options.type) ? Ptz::Capability::continuousPanTiltZoom
                                             : Ptz::Capability::none;
    }

    std::optional<Type> lastMoveType;

private:
    Types m_types;
};

TEST(FallbackPtzController, aggregatesSupportedTypes)
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

} // namespace
