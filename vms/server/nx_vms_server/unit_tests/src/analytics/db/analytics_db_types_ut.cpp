#include <gtest/gtest.h>

#include <nx/utils/random.h>

#include <analytics/db/analytics_db_types.h>

#include "attribute_dictionary.h"

namespace nx::analytics::db::test {

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
    setFreeText("Hello world with empty  spaces");
    serializeFilter();
    assertSerializedFreeTextDoesNotContainSpaces();
}

class FilterTest: public ::testing::Test
{
protected:
    static nx::common::metadata::ObjectMetadata sampleMetadata()
    {
        nx::common::metadata::ObjectMetadata sample;
        sample.trackId = QnUuid::createUuid();
        sample.bestShot = false;
        sample.boundingBox = QRectF(0, 0, 1, 1);
        sample.typeId = "nx.Car";
        sample.attributes.emplace_back("Type", "Truck");
        sample.attributes.emplace_back("Brand", "Mazda");
        return sample;
    }

    static ObjectTrack sampleTrack()
    {
        ObjectPosition samplePosition;
        samplePosition.boundingBox = QRectF(0, 0, 1, 1);

        ObjectTrack sample;
        sample.id = QnUuid::createUuid();
        sample.deviceId = QnUuid::createUuid();
        sample.firstAppearanceTimeUs = 0;
        sample.lastAppearanceTimeUs = std::numeric_limits<qint64>::max();
        sample.objectPosition.add(samplePosition.boundingBox);
        sample.objectTypeId = "nx.Car";
        sample.attributes.emplace_back("Type", "Truck");
        sample.attributes.emplace_back("Brand", "Mazda");
        return sample;
    }
};

using FreeTextAndExpectedResult = std::pair<std::string, bool>;

class FilterByUserInputTest:
    public FilterTest,
    public ::testing::WithParamInterface<FreeTextAndExpectedResult>
{
protected:
	std::string userInput() const { return GetParam().first; }
    bool expectedResult() const { return GetParam().second; }
};

TEST_P(FilterByUserInputTest, metadata)
{
    Filter filter;
    filter.loadUserInputToFreeText(QString::fromStdString(userInput()));
    ASSERT_EQ(filter.acceptsMetadata(sampleMetadata()), expectedResult());
}

TEST_P(FilterByUserInputTest, track)
{
    Filter filter;
    filter.loadUserInputToFreeText(QString::fromStdString(userInput()));
    ASSERT_EQ(filter.acceptsTrack(sampleTrack()), expectedResult());
}

static std::vector<FreeTextAndExpectedResult> kUserInputAndExpectedResults{
    {"Truck", true},
    {"Tru", true}, //< Support prefix search.
    {"tru", true}, //< Search is case-insensitive.
    {"ruck", false}, //< Search by the word end is not supported in fts4.
    {"Ma*a", false}, //< Wildcards are not supported in fts4.
    {"Ma?da", false}, //< Wildcards are not supported in fts4.
    {"Type", false}, //< Search by attribute values only.
    {"Truck Mazda", true}, //< AND logic is applied. Positive case.
    {"Brand Mazda", false}, //< AND logic is applied. Negative case.
    {"Mazda Truck", true}, //< Order is not important.
    {"MA TR", true}, //< Each word is prefixed.
};

INSTANTIATE_TEST_CASE_P(AnalyticsDbFilter,
	FilterByUserInputTest,
	::testing::ValuesIn(kUserInputAndExpectedResults));

} // namespace nx::analytics::db::test
