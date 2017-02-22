#include <gtest/gtest.h>

#include <future>
#include <boost/optional.hpp>

#include <nx/utils/std/thread.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/random.h>

TEST(MoveOnlyFunc, common)
{
    for (int i = 0; i < 100; ++i)
    {
        nx::utils::MoveOnlyFunc< void() > handler;
        boost::optional< nx::utils::promise< bool > > promise;
        int y = nx::utils::random::number();
        {
            promise = nx::utils::promise< bool >();
            handler = [&, y]() { promise->set_value(true); };
        }

        handler();
        ASSERT_TRUE(promise->get_future().get());
    }
}
