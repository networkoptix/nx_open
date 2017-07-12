#pragma once

#include <gtest/gtest.h>

#include <nx/network/stun/abstract_async_client.h>

namespace nx {
namespace stun {
namespace test {

template<typename ClientType>
class StunAsyncClientAcceptanceTest:
    public ::testing::Test
{
public:
    ~StunAsyncClientAcceptanceTest()
    {
        m_client.pleaseStopSync();
    }

protected:
    void givenClientWithIndicationHandler()
    {
        ASSERT_TRUE(addIndicationHandler());
    }

    void whenRemovedHandler()
    {
        nx::utils::promise<void> done;
        m_client.cancelHandlers(this, [&done]() { done.set_value(); });
        done.get_future().wait();
    }

    void thenSameHandlerCannotBeAdded()
    {
        ASSERT_FALSE(addIndicationHandler());
    }

    void thenSameHandlerCanBeAddedAgain()
    {
        ASSERT_TRUE(addIndicationHandler());
    }

private:
    ClientType m_client;

    bool addIndicationHandler()
    {
        return m_client.setIndicationHandler(
            nx::stun::MethodType::userIndication + 1,
            [](nx::stun::Message) {},
            this);
    }
};

TYPED_TEST_CASE_P(StunAsyncClientAcceptanceTest);

TYPED_TEST_P(StunAsyncClientAcceptanceTest, same_handler_cannot_be_added_twice)
{
    givenClientWithIndicationHandler();
    thenSameHandlerCannotBeAdded();
}

TYPED_TEST_P(StunAsyncClientAcceptanceTest, add_remove_indication_handler)
{
    givenClientWithIndicationHandler();
    whenRemovedHandler();
    thenSameHandlerCanBeAddedAgain();
}

REGISTER_TYPED_TEST_CASE_P(StunAsyncClientAcceptanceTest, 
    same_handler_cannot_be_added_twice, add_remove_indication_handler);

} // namespace test
} // namespace stun
} // namespase nx
