#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>
#include <test_support/analytics/storage/analytics_storage_types.h>

namespace nx {
namespace analytics {
namespace storage {
namespace test {

class AnalyticsStorageTypesFilter:
    public ::testing::Test
{
public:
    AnalyticsStorageTypesFilter()
    {
        m_filter = generateRandomFilter();
    }

protected:
    void setFreeText(const QString& str)
    {
        m_filter.freeText = str;
    }

    void serializeFilter()
    {
        serializeToParams(m_filter, &m_serializedFilter);
    }

    void assertSerializedFreeTextDoesNotContainSpaces()
    {
        const auto freeText = m_serializedFilter.value("freeText");
        ASSERT_FALSE(freeText.isEmpty());
        ASSERT_EQ(-1, freeText.indexOf(' '));
    }

    void assertSameAfterSerializeAndDeserialize()
    {
        serializeFilter();

        Filter restoredFilter;
        deserializeFromParams(m_serializedFilter, &restoredFilter);

        ASSERT_EQ(m_filter, restoredFilter);
    }

private:
    Filter m_filter;
    QnRequestParamList m_serializedFilter;
};

TEST_F(AnalyticsStorageTypesFilter, serialize_and_deserialize_are_symmetric)
{
    assertSameAfterSerializeAndDeserialize();
}

TEST_F(AnalyticsStorageTypesFilter, freeText_does_not_contain_spaces_after_serialize)
{
    setFreeText(lit("Hello world with empty  spaces"));
    serializeFilter();
    assertSerializedFreeTextDoesNotContainSpaces();
}

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace nx
