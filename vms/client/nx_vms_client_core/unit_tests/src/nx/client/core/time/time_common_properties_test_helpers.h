#pragma once

#include <nx/client/core/time/time_constants.h>

template<typename ObjectType>
void checkDisplayOffsetPropertyInteractionAndRanges(ObjectType* object)
{
    using TimeConstants = nx::client::core::TimeConstants;

    // Checks if display offset setter/getter works correctly.
    object->setDisplayOffset(TimeConstants::kMinDisplayOffset);
    ASSERT_EQ(TimeConstants::kMinDisplayOffset, object->displayOffset());

    // Checks if minimum display offset value is constrained.
    object->setDisplayOffset(TimeConstants::kMinDisplayOffset - 1);
    ASSERT_EQ(TimeConstants::kMinDisplayOffset, object->displayOffset());

    // Checks if maximum display offset value is constrained.
    object->setDisplayOffset(TimeConstants::kMaxDisplayOffset + 1);
    ASSERT_EQ(TimeConstants::kMaxDisplayOffset, object->displayOffset());
}

template<typename ObjectType>
void checkPositionPropertyInteractionAndRanges(ObjectType* object)
{
    // Checks if current position setter/getter works correctly.
    static const int kSomePosition = 1000;
    object->setPosition(kSomePosition);
    ASSERT_EQ(kSomePosition, object->position());

    // Checks if minimum position value is constrained.
    object->setPosition(-1);
    ASSERT_EQ(object->position(), 0);
}
