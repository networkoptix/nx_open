#include <functional>
#include <gtest/gtest.h>
#include <nx/update/info/async_update_checker.h>
#include <nx/update/info/abstract_update_registry.h>

#include "../../inl.h"

namespace nx {
namespace update {
namespace info {
namespace test {

static const QString kDummyBaseUrl = "dummy";

class AsyncUpdateChecker: public ::testing::Test
{
protected:
    void whenAsyncCheckRequestHasBeenIssued()
    {
        using namespace std::placeholders;
        m_asyncUpdateChecker.check(
            kDummyBaseUrl,
            std::bind(&AsyncUpdateChecker::onCheckCompleted, this, _1, _2));
    }

    void thenNonEmptyAbstractUpdateRegistryPtrShouldBeReturned()
    {

    }
private:
    info::AsyncUpdateChecker m_asyncUpdateChecker;
    AbstractUpdateRegistryPtr m_updateRegistry;

    void onCheckCompleted(ResultCode resultCode, AbstractUpdateRegistryPtr updateRegistry)
    {
        ASSERT_EQ(ResultCode::ok, resultCode);
        ASSERT_TRUE((bool) updateRegistry);
    }
};

TEST_F(AsyncUpdateChecker, AsyncOperationCompletesSuccessfully)
{
    whenAsyncCheckRequestHasBeenIssued();
    thenNonEmptyAbstractUpdateRegistryPtrShouldBeReturned();
}

} // namespace test
} // namespace info
} // namespace update
} // namespace nx
