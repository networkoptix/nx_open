
#include <limits>

#include <gtest/gtest.h>

#include <managers/time_manager.h>


TEST(TimePriorityKey, common)
{
    ec2::TimePriorityKey key1;
    key1.sequence = 1;
    ec2::TimePriorityKey key2;
    key2.sequence = 2;

    ASSERT_TRUE(key1 < key2);
    ASSERT_FALSE(key2 < key1);

    key2.sequence = std::numeric_limits<decltype(ec2::TimePriorityKey::sequence)>::max()-1;
    ASSERT_FALSE(key1 < key2);
    ASSERT_TRUE(key2 < key1);
}
