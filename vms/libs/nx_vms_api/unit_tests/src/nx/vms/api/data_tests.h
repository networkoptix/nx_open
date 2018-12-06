#pragma once

#include <gtest/gtest.h>

#include <functional>

#include <boost/preprocessor/seq/for_each.hpp>

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::test {

template<typename Data>
void testDataEqual(const Data& sample)
{
    Data deserialized;
    EXPECT_TRUE(QJson::deserialize(QJson::serialized(sample), &deserialized));
    EXPECT_EQ(sample, deserialized);

    Data constructedCopy(sample);
    EXPECT_EQ(sample, constructedCopy);

    Data assignedCopy;
    assignedCopy = sample;
    EXPECT_EQ(sample, constructedCopy);

    Data constructedMove(std::move(constructedCopy));
    EXPECT_EQ(sample, constructedMove);

    Data assignedMove;
    assignedMove = std::move(assignedCopy);
    EXPECT_EQ(sample, assignedMove);
}

template<typename Data, typename Modifier>
void testDataNotEqual(const Data& sample, const Modifier& modify, const char* description)
{
    Data modified(sample);
    modify(&modified);
    EXPECT_NE(sample, modified) << description;
}

#define NX_VMS_API_PRINT_TO(DATA) \
    void PrintTo(const DATA& value, ::std::ostream* os) \
    { \
        *os << QJson::serialized(value).toStdString(); \
    }

#define NX_VMS_API_TEST_NOT_EQUAL(R, SAMPLE, MODIFIER) \
    testDataNotEqual(SAMPLE, [](auto* data) { data->MODIFIER; }, #MODIFIER);

#define NX_VMS_API_DATA_TEST(DATA, INITIALIZER, MODIFIERS) \
    TEST(Api ## DATA, INITIALIZER) \
    { \
        const DATA sample(INITIALIZER()); \
        testDataEqual(sample); \
        BOOST_PP_SEQ_FOR_EACH(NX_VMS_API_TEST_NOT_EQUAL, sample, MODIFIERS) \
    }

} // namespace nx::vms::api::test
