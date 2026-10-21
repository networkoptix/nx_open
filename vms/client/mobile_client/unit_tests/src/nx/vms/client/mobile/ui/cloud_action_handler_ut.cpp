// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/mobile/ui/cloud_action_handler.h>
#include <nx/vms/client/mobile/ui/detail/qml_result_watcher.h>

#include "fake_result_screen.h"

namespace nx::vms::client::mobile::test {

namespace {

using Error = core::OauthDataRequestError;
using Result = std::optional<Error>;
using Results = std::vector<Result>;

/** Opens fake screens instead of the cloud login screen. */
class TestCloudActionHandler: public CloudActionHandler
{
public:
    bool canOpenScreen = true;
    std::vector<std::unique_ptr<FakeResultScreen>> screens;

protected:
    virtual detail::QmlResultWatcher* openCloudLoginScreen() override
    {
        if (!canOpenScreen)
            return nullptr;

        screens.push_back(std::make_unique<FakeResultScreen>());
        return new detail::QmlResultWatcher(screens.back().get());
    }
};

class CloudActionHandlerTest: public ::testing::Test
{
protected:
    /** Requests the re-authorization and collects the handler results to the given list. */
    void request(QObject* context, Results* results)
    {
        handler.requestCloudReauthorization(
            context, [results](Result result) { results->push_back(result); });
    }

    void reportResult(const QString& result) { emit handler.screens.back()->gotResult(result); }

protected:
    TestCloudActionHandler handler;
};

} // namespace

TEST_F(CloudActionHandlerTest, requestsShareScreen)
{
    QObject context;
    Results first;
    Results second;
    request(&context, &first);
    request(&context, &second);
    ASSERT_EQ(handler.screens.size(), 1u);

    reportResult("success");
    EXPECT_EQ(first, Results{std::nullopt});
    EXPECT_EQ(second, Results{std::nullopt});
}

TEST_F(CloudActionHandlerTest, handlerIsNotCalledAfterContextDestroyed)
{
    auto destroyedContext = std::make_unique<QObject>();
    QObject context;
    Results destroyedContextResults;
    Results results;
    request(destroyedContext.get(), &destroyedContextResults);
    request(&context, &results);

    destroyedContext.reset();
    reportResult("success");

    EXPECT_TRUE(destroyedContextResults.empty());
    EXPECT_EQ(results, Results{std::nullopt});
}

TEST_F(CloudActionHandlerTest, noParentIsReportedSynchronously)
{
    handler.canOpenScreen = false;

    QObject context;
    Results results;
    request(&context, &results);

    // No events are processed, so the result can only be reported synchronously.
    EXPECT_EQ(results, Results{Error::noParent});
}

TEST_F(CloudActionHandlerTest, newScreenAfterResult)
{
    QObject context;
    Results first;
    request(&context, &first);
    reportResult("rejected");
    EXPECT_EQ(first, Results{Error::dialogRejected});

    Results second;
    request(&context, &second);
    ASSERT_EQ(handler.screens.size(), 2u);
    EXPECT_TRUE(second.empty());

    reportResult("success");
    EXPECT_EQ(second, Results{std::nullopt});
    EXPECT_EQ(first, Results{Error::dialogRejected});
}

} // namespace nx::vms::client::mobile::test
