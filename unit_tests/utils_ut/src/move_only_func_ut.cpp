
#include <gtest/gtest.h>

#include <future>

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>


TEST(MoveOnlyFunc, common)
{
    for (int i = 0; i < 100; ++i)
    {
        nx::utils::MoveOnlyFunc< void() > handler;
        boost::optional< std::promise< bool > > promise;
        int y = rand();
        {
            promise = std::promise< bool >();
            handler = [&, y]() { promise->set_value(true); };
        }

        handler();
        ASSERT_TRUE(promise->get_future().get());
    }
}
