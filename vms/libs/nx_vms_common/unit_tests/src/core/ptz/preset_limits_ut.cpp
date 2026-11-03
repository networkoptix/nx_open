// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QThreadPool>
#include <QtTest/QSignalSpy>

#include <core/ptz/basic_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/thread_safe_ptz_controller.h>
#include <core/ptz/threaded_ptz_controller.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::common::ptz::test {

namespace {

class PresetLimitsController: public QnBasicPtzController
{
public:
    PresetLimitsController(): QnBasicPtzController({}) {}

    virtual bool getPresetLimits(api::PtzPresetLimits* limits) const override
    {
        ++requests;
        if (!succeeds)
            return false;
        *limits = presetLimits;
        return true;
    }

    api::PtzPresetLimits presetLimits;
    bool succeeds = true;
    mutable int requests = 0;
};

} // namespace

TEST(PtzPresetLimits, DefaultControllerReportsUnknown)
{
    QnBasicPtzController controller({});
    api::PtzPresetLimits limits{.maximumNativePresetCount = 128};
    ASSERT_TRUE(controller.getPresetLimits(&limits));
    EXPECT_FALSE(limits.maximumNativePresetCount);
}

TEST(PtzPresetLimits, CapacityIsQueryableWithoutPresetOperations)
{
    PresetLimitsController controller;
    EXPECT_FALSE(controller.supports(Command::getPresets));
    EXPECT_TRUE(controller.supports(Command::getPresetLimits));
    controller.presetLimits.maximumNativePresetCount = 0;
    api::PtzPresetLimits limits;
    ASSERT_TRUE(controller.getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 0);
}

TEST(PtzPresetLimits, WrappersPreserveNativeCapacityWithSystemPresets)
{
    auto device = QSharedPointer<PresetLimitsController>::create();
    auto presets = QSharedPointer<QnPresetPtzController>::create(device);
    auto proxy = QSharedPointer<QnProxyPtzController>::create(presets);
    ThreadSafePtzController controller(proxy);

    for (const auto maximum:
        {std::optional<int>{}, std::optional<int>{0}, std::optional<int>{128}})
    {
        device->presetLimits.maximumNativePresetCount = maximum;
        api::PtzPresetLimits limits;
        ASSERT_TRUE(controller.getPresetLimits(&limits));
        EXPECT_EQ(limits.maximumNativePresetCount, maximum);
    }

    device->succeeds = false;
    api::PtzPresetLimits limits;
    EXPECT_FALSE(controller.getPresetLimits(&limits));
}

TEST(PtzPresetLimits, LegacyAggregateDoesNotReadCapacity)
{
    auto device = QSharedPointer<PresetLimitsController>::create();
    device->presetLimits.maximumNativePresetCount = 128;
    QnPtzData data;
    ASSERT_TRUE(device->getData(&data, DataField::all, {Type::operational}));
    EXPECT_EQ(device->requests, 0);

    Command command = Command::invalid;
    EXPECT_FALSE(nx::reflect::fromString("GetPresetLimitsPtzCommand", &command));
}

TEST(PtzPresetLimits, ResponseSerializationPreservesUnknownZeroAndPositiveCapacity)
{
    for (const auto maximum:
        {std::optional<int>{}, std::optional<int>{0}, std::optional<int>{128}})
    {
        const api::PtzPresetLimits limits{.maximumNativePresetCount = maximum};
        const auto restored = QJson::deserialized<api::PtzPresetLimits>(QJson::serialized(limits));
        EXPECT_EQ(restored.maximumNativePresetCount, maximum);
    }
}

TEST(PtzPresetLimits, ThreadedCompletionDistinguishesUnknownAndFailure)
{
    auto device = QSharedPointer<PresetLimitsController>::create();
    QThreadPool pool;
    QnThreadedPtzController controller(device, &pool);
    QSignalSpy finished(&controller, &QnAbstractPtzController::finished);

    for (const auto maximum:
        {std::optional<int>{}, std::optional<int>{0}, std::optional<int>{128}})
    {
        device->presetLimits.maximumNativePresetCount = maximum;
        ASSERT_TRUE(controller.getPresetLimits(nullptr));
        ASSERT_TRUE(!finished.empty() || finished.wait(1000));
        const auto arguments = finished.takeFirst();
        EXPECT_EQ(arguments[0].value<Command>(), Command::getPresetLimits);
        ASSERT_TRUE(arguments[1].isValid());
        EXPECT_EQ(arguments[1].value<api::PtzPresetLimits>().maximumNativePresetCount, maximum);
    }

    device->succeeds = false;
    ASSERT_TRUE(controller.getPresetLimits(nullptr));
    ASSERT_TRUE(!finished.empty() || finished.wait(1000));
    EXPECT_FALSE(finished.takeFirst()[1].isValid());
    pool.waitForDone();
}

} // namespace nx::vms::common::ptz::test
