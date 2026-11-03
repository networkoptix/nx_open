// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>

#include <core/ptz/basic_ptz_controller.h>
#include <nx/network/rest/params.h>
#include <nx/network/rest/result.h>
#include <nx/vms/client/core/ptz/caching_ptz_controller.h>
#include <nx/vms/client/core/ptz/remote_ptz_controller.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

namespace nx::vms::client::core::ptz::test {

using namespace nx::vms::common::ptz;

namespace {

class AsyncPresetLimitsController: public QnBasicPtzController
{
public:
    AsyncPresetLimitsController(const QnResourcePtr& resource = {}): QnBasicPtzController(resource)
    {
    }

    virtual Ptz::Capabilities getCapabilities(const Options&) const override
    {
        return Ptz::Capability::asynchronous;
    }

    virtual bool getPresetLimits(api::PtzPresetLimits*) const override
    {
        ++requests;
        return acceptsRequests;
    }

    virtual bool getPresets(QnPtzPresetList*) const override
    {
        ++presetRequests;
        return acceptsRequests;
    }

    virtual bool getTours(QnPtzTourList*) const override
    {
        ++tourRequests;
        return acceptsRequests;
    }

    virtual bool getData(QnPtzData*, DataFields query, const Options&) const override
    {
        aggregateQuery = query;
        return true;
    }

    void complete(std::optional<int> maximum)
    {
        emit finished(Command::getPresetLimits,
            QVariant::fromValue(api::PtzPresetLimits{.maximumNativePresetCount = maximum}));
    }

    void fail(Command command = Command::getPresetLimits) { emit finished(command, {}); }

    void completeList(Command command)
    {
        emit finished(command,
            command == Command::getPresets
                ? QVariant::fromValue(QnPtzPresetList{{"preset", "Preset"}})
                : QVariant::fromValue(QnPtzTourList{{"tour", "Tour", {}}}));
    }

    void completeData()
    {
        QnPtzData data(DataField::all, DataField::none);
        emit finished(Command::getData, QVariant::fromValue(data));
    }

    bool acceptsRequests = true;
    mutable int requests = 0;
    mutable int presetRequests = 0;
    mutable int tourRequests = 0;
    mutable DataFields aggregateQuery;
};

class TestRemotePtzController: public RemotePtzController
{
public:
    TestRemotePtzController(): RemotePtzController({}) {}
    using RemotePtzController::makePresetLimitsCompletion;
    using RemotePtzController::processResponse;

    virtual bool getPresetLimits(api::PtzPresetLimits*) const override
    {
        completion = makePresetLimitsCompletion();
        ++requests;
        return true;
    }

    virtual void invalidate() override { ++invalidations; }
    int invalidations = 0;
    mutable int requests = 0;
    mutable nx::MoveOnlyFunc<void(QVariant)> completion;
};

} // namespace

TEST(PtzPresetLimitsCache, AggregateQueryPreservesOlderServerWireContract)
{
    auto device = QSharedPointer<AsyncPresetLimitsController>::create();
    CachingPtzController cache(device);
    QnPtzData data;
    cache.getData(&data, DataField::all, {Type::operational});

    // Use the same query serialization as RemotePtzController::getData.
    nx::network::rest::Params params;
    params.insert("query", device->aggregateQuery);
    EXPECT_EQ(params["query"],
        "capabilities|devicePosition|logicalPosition|deviceLimits|logicalLimits|flip|presets|tours|"
        "activeObject|homeObject|auxiliaryTraits");

    DataFields query;
    EXPECT_FALSE(nx::reflect::fromString("presetLimits", &query));

    api::PtzPresetLimits limits;
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 1);
    device->complete(128);
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 128);
}

TEST(PtzPresetLimitsCache, CachesUnknownZeroAndPositiveResults)
{
    for (const auto maximum:
        {std::optional<int>{}, std::optional<int>{0}, std::optional<int>{128}})
    {
        auto device = QSharedPointer<AsyncPresetLimitsController>::create();
        CachingPtzController cache(device);
        api::PtzPresetLimits limits;

        EXPECT_FALSE(cache.getPresetLimits(&limits));
        EXPECT_FALSE(cache.getPresetLimits(&limits));
        EXPECT_EQ(device->requests, 1); //< Do not duplicate a pending request.

        device->complete(maximum);
        ASSERT_TRUE(cache.getPresetLimits(&limits));
        EXPECT_EQ(limits.maximumNativePresetCount, maximum);

        device->completeData();
        device->completeList(Command::getPresets);
        device->completeList(Command::getTours);
        ASSERT_TRUE(cache.getPresetLimits(&limits));
        EXPECT_EQ(limits.maximumNativePresetCount, maximum);
        EXPECT_EQ(device->requests, 1);
    }
}

TEST(PtzPresetLimitsCache, InvalidatedCapacityCanBecomeUnknown)
{
    auto device = QSharedPointer<AsyncPresetLimitsController>::create();
    CachingPtzController cache(device);
    api::PtzPresetLimits limits;
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    device->complete(128);
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 128);

    device->completeData();
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 128);

    cache.invalidate();
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 2);
    device->complete(std::nullopt);
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    EXPECT_FALSE(limits.maximumNativePresetCount);
    EXPECT_EQ(device->requests, 2);
}

TEST(PtzPresetLimitsCache, FailedRequestsRemainUnavailableAndCanBeRetried)
{
    auto device = QSharedPointer<AsyncPresetLimitsController>::create();
    CachingPtzController cache(device);
    api::PtzPresetLimits limits;
    device->acceptsRequests = false;
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 2);

    device->acceptsRequests = true;
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    device->fail();
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 4);

    device->complete(128);
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    cache.invalidate();
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 5);
    device->fail();
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 6);
    device->complete(64);
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 64);
}

TEST(PtzPresetLimitsCache, InvalidationDiscardsPendingResponse)
{
    auto device = QSharedPointer<AsyncPresetLimitsController>::create();
    CachingPtzController cache(device);
    api::PtzPresetLimits limits;
    QVariant completion;
    QObject::connect(&cache,
        &QnAbstractPtzController::finished,
        &cache,
        [&](Command command, const QVariant& data)
        {
            if (command == Command::getPresetLimits)
                completion = data;
        });

    EXPECT_FALSE(cache.getPresetLimits(&limits));
    cache.invalidate();
    cache.invalidate();
    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 1); //< Drain the old request before issuing another.
    device->complete(128);
    EXPECT_FALSE(completion.isValid());

    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 2);
    device->complete(64);
    ASSERT_TRUE(completion.isValid());
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 64);
}

TEST(PtzPresetLimitsCache, ResourceSignalsInvalidateCapacityWithoutCapabilityChanges)
{
    const QnVirtualCameraResourcePtr camera(new CameraResourceStub());
    auto device = QSharedPointer<AsyncPresetLimitsController>::create(camera);
    const QnSharedResourcePointer<CachingPtzController> cache(new CachingPtzController(device));
    cache->initialize();
    device->completeData(); //< Lifecycle invalidation must also work after initialization.

    api::PtzPresetLimits limits;
    EXPECT_FALSE(cache->getPresetLimits(&limits));
    device->complete(128);
    ASSERT_TRUE(cache->getPresetLimits(&limits));

    // Exercise the offline/online notifications without a server-backed status dictionary.
    emit camera->statusChanged(camera, Qn::StatusChangeReason::Local);
    EXPECT_FALSE(cache->getPresetLimits(&limits));
    emit camera->statusChanged(camera, Qn::StatusChangeReason::Local);
    device->complete(128); //< A response from before reconnect must not repopulate the cache.
    EXPECT_FALSE(cache->getPresetLimits(&limits));
    device->complete(64);
    ASSERT_TRUE(cache->getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 64);

    camera->setParentId(nx::Uuid::createUuid());
    EXPECT_FALSE(cache->getPresetLimits(&limits));
    device->complete(32);
    ASSERT_TRUE(cache->getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 32);
}

TEST(PtzPresetLimitsCache, ListGettersPreserveAcceptanceAndFailedRefreshCadence)
{
    for (const auto command: {Command::getPresets, Command::getTours})
    {
        auto device = QSharedPointer<AsyncPresetLimitsController>::create();
        CachingPtzController cache(device);
        QnPtzPresetList presets;
        QnPtzTourList tours;
        const auto getList = [&]()
        {
            return command == Command::getPresets ? cache.getPresets(&presets)
                                                  : cache.getTours(&tours);
        };
        const auto& requests =
            command == Command::getPresets ? device->presetRequests : device->tourRequests;

        device->acceptsRequests = false;
        EXPECT_FALSE(getList());
        EXPECT_FALSE(getList());
        EXPECT_EQ(requests, 2);

        device->acceptsRequests = true;
        EXPECT_TRUE(getList()); //< Existing list API reports request acceptance before completion.
        EXPECT_EQ(requests, 3);
        device->completeList(command);
        ASSERT_TRUE(getList());
        if (command == Command::getPresets)
            EXPECT_EQ(presets, (QnPtzPresetList{{"preset", "Preset"}}));
        else
            EXPECT_EQ(tours, (QnPtzTourList{{"tour", "Tour", {}}}));
        EXPECT_EQ(requests, 3);

        cache.invalidate();
        ASSERT_TRUE(getList());
        EXPECT_EQ(requests, 4);
        device->fail(command);
        EXPECT_TRUE(getList());
        EXPECT_TRUE(getList());
        EXPECT_EQ(requests, 4); //< Cached list failures retain the existing refresh deadline.
    }
}

TEST(RemotePtzPresetLimits, CompletionDistinguishesUnknownZeroPositiveAndFailure)
{
    TestRemotePtzController controller;
    QVariant result;
    QObject::connect(&controller,
        &QnAbstractPtzController::finished,
        &controller,
        [&](Command command, const QVariant& data)
        {
            EXPECT_EQ(command, Command::getPresetLimits);
            result = data;
        });

    for (const auto maximum:
        {std::optional<int>{}, std::optional<int>{0}, std::optional<int>{128}})
    {
        controller.processResponse(Command::getPresetLimits,
            QVariant::fromValue(api::PtzPresetLimits{.maximumNativePresetCount = maximum}));
        ASSERT_TRUE(result.isValid());
        EXPECT_EQ(result.value<api::PtzPresetLimits>().maximumNativePresetCount, maximum);
    }
    EXPECT_EQ(controller.invalidations, 0);

    controller.processResponse(Command::getPresetLimits, {});
    EXPECT_FALSE(result.isValid());
    EXPECT_EQ(controller.invalidations, 1);
}

TEST(RemotePtzPresetLimits, CanceledRequestReleasesCacheAfterInvalidation)
{
    auto device = QSharedPointer<TestRemotePtzController>::create();
    CachingPtzController cache(device);
    const int initialInvalidations = device->invalidations;
    api::PtzPresetLimits limits;

    EXPECT_FALSE(cache.getPresetLimits(&limits));
    cache.invalidate();
    device->completion = {}; //< Model ServerConnection destroying its pending callback.
    QCoreApplication::processEvents();

    EXPECT_FALSE(cache.getPresetLimits(&limits));
    EXPECT_EQ(device->requests, 2);
    device->completion(QVariant::fromValue(api::PtzPresetLimits{.maximumNativePresetCount = 64}));
    device->completion = {};
    QCoreApplication::processEvents();
    ASSERT_TRUE(cache.getPresetLimits(&limits));
    EXPECT_EQ(limits.maximumNativePresetCount, 64);
    EXPECT_EQ(device->requests, 2);
    EXPECT_EQ(device->invalidations, initialInvalidations + 2);
}

TEST(RemotePtzPresetLimits, CompletionGuardIsSafeAfterControllerDestruction)
{
    auto controller = std::make_unique<TestRemotePtzController>();
    auto canceled = controller->makePresetLimitsCompletion();
    auto completed = controller->makePresetLimitsCompletion();
    controller.reset();

    completed(QVariant::fromValue(api::PtzPresetLimits{.maximumNativePresetCount = 128}));
    canceled = {};
    completed = {};
    QCoreApplication::processEvents();
}

} // namespace nx::vms::client::core::ptz::test
