// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "algorithm.h"

#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace nx::sdk::cloud_storage {

using namespace std::chrono;

namespace {

template<typename T>
auto orderPred(bool isAscending, const T& c1, const T& c2)
{
    if (isAscending)
        return c1 < c2;

    return c2 < c1;
}

static constexpr int kMotionGridWidth = 44;
static constexpr int kMotionGridHeight = 32;
static const int kGridDataSizeBytes = kMotionGridWidth * kMotionGridHeight / 8;
static const Rect kMaxGridRect(0, 0, kMotionGridWidth, kMotionGridHeight);

void setBit(uint8_t* data, int x, int y)
{
    bool correctData = (x >= 0 && x < kMotionGridWidth) && (y >= 0 && y < kMotionGridHeight);

    if (!correctData)
        return;

    int offset = (x * kMotionGridHeight + y) / 8;
    data[offset] |= 0x80 >> (y & 7);
}

typedef struct
{
    int64_t one;
    uint64_t two;
} simd128i;

typedef struct
{
    uint64_t one;
    uint64_t two;
} simd128;

class RegionMatcher
{
public:
    RegionMatcher(const Rect& rect)
    {
        memset(m_mask, 0, kMotionGridWidth * kMotionGridHeight / 8);
        m_maskStart = std::round(
            std::min<double>((rect.left() * kMotionGridHeight + rect.top()) / 128,
                kMotionGridWidth * kMotionGridHeight / 128 - 1));
        m_maskEnd = std::round(
            std::max<double>(((rect.right() - 1) * kMotionGridHeight + rect.bottom() - 1) / 128,
                m_maskEnd));
        for (int x = (int) rect.left(); x <= rect.right(); ++x)
        {
            for (int y = (int) rect.top(); y <= rect.bottom(); ++y)
            {
                setBit((uint8_t*) &m_mask, x, y);
            }
        }
    }

    bool match(const std::vector<uint8_t>& data) const
    {
        uint64_t* curPtr = (uint64_t*) data.data();
        curPtr += m_maskStart * 2;
        uint64_t* maskPtr = (uint64_t*) &m_mask;
        maskPtr += m_maskStart * 2;

        for (int i = m_maskStart; i <= m_maskEnd; ++i)
        {
            if (*curPtr++ & *maskPtr++)
                return true;
            if (*curPtr++ & *maskPtr++)
                return true;
        }

        return false;
    }

private:
    int m_maskStart = 0;
    int m_maskEnd = 0;
    simd128i m_mask[kGridDataSizeBytes / sizeof(simd128i)];
};

bool hasAttributeWithName(const std::string& name, const std::vector<Attribute>& attributes)
{
    if (name.empty())
        return false;

    for (const auto& a: attributes)
    {
        if (a.name.empty() || name.size() > a.name.size())
            continue;

        size_t i = 0;
        for (; i < name.size(); ++i)
        {
            if (name[i] != a.name[i])
                break;
        }

        if (i == a.name.size() || a.name[i] == '.')
            return true;
    }

    return false;
}

bool hasAttributeWithValue(const std::string& value, const std::vector<Attribute>& attributes)
{
    if (value.empty())
        return true;

    for (const auto& a: attributes)
    {
        if (a.value.find(value) != std::string::npos)
            return true;
    }

    return false;
}

bool hasAttributeWithRange(
    const nx::sdk::cloud_storage::NumericRange& range, const std::vector<Attribute>& attributes)
{
    for (const auto& a: attributes)
    {
        const auto maybeAttrRange = NumericRange::fromString(a.value);
        if (maybeAttrRange && maybeAttrRange->intersects(range))
            return true;
    }

    return false;
}

} // namespace

std::vector<std::string> split(const std::string& original, const std::string& separator)
{
    std::vector<std::string> result;
    enum class State
    {
        body,
        separator,
    } state = State::body;

    if (separator.empty())
    {
        result.push_back(original);
        return result;
    }

    size_t pos = 0;
    size_t sepPos = 0;
    std::string next;
    bool done = false;
    while (!done)
    {
        switch (state)
        {
            case State::body:
                if (original[pos] == separator[sepPos])
                {
                    state = State::separator;
                    if (!next.empty())
                        result.push_back(next);

                    next.clear();
                    break;
                }

                next.push_back(original[pos]);
                if (pos == original.size() - 1)
                {
                    result.push_back(next);
                    done = true;
                }

                ++pos;
                break;
            case State::separator:
                ++pos;
                ++sepPos;
                if (pos == original.size())
                {
                    done = true;
                    break;
                }

                if (sepPos == separator.size())
                {
                    sepPos = 0;
                    state = State::body;
                }

                break;
        }
    }

    return result;
}

bool bookmarkMatches(const Bookmark& bookmark, const BookmarkFilter& filter)
{
    if (filter.id && bookmark.id != *filter.id)
        return false;

    if (!filter.deviceIds.empty())
    {
        const bool deviceMatches = std::any_of(
            filter.deviceIds.cbegin(), filter.deviceIds.cend(),
            [&bookmark](const auto& id) { return id == bookmark.deviceId; });

        if (!deviceMatches)
            return false;
    }

    using namespace std::chrono;
    const auto toCheckAgainstPeriod = TimePeriod(
        filter.startTimestamp ? *filter.startTimestamp : 0ms,
        filter.endTimestamp
            ? *filter.endTimestamp - (filter.startTimestamp ? *filter.startTimestamp : 0ms)
            : -1ms);

    const auto bookmarkPeriod = TimePeriod(bookmark.startTimestamp, bookmark.duration);
    if (!bookmarkPeriod.intersects(toCheckAgainstPeriod))
        return false;

    if (filter.creationStartTimestamp
        && bookmark.creationTimestamp < filter.creationStartTimestamp)
    {
        return false;
    }

    if (filter.creationEndTimestamp && bookmark.creationTimestamp > filter.creationEndTimestamp)
        return false;

    if (filter.minVisibleLength && bookmark.duration < filter.minVisibleLength)
        return false;

    if (filter.text)
    {
        if (bookmark.name.find(*filter.text) == std::string::npos)
        {
            if (bookmark.tags.empty())
                return false;

            const bool hasMatchingTag = std::any_of(bookmark.tags.cbegin(),
                bookmark.tags.cend(),
                [&filter](const auto& tag)
                { return tag.find(*filter.text) != std::string::npos; });

            if (!hasMatchingTag)
                return false;
        }
    }

    return true;
}

void sortAndLimitBookmarks(const BookmarkFilter& filter, std::vector<Bookmark>* outBookmarks)
{
    const bool isAsc = filter.order == SortOrder::ascending;
    std::sort(
        outBookmarks->begin(), outBookmarks->end(),
        [&filter, isAsc](const auto& b1, const auto& b2)
        {
            using Column = BookmarkFilter::SortColumn;
            switch (filter.column)
            {
                case Column::creationTime:
                    return orderPred(isAsc, b1.creationTimestamp, b2.creationTimestamp);
                case Column::description:
                    return orderPred(isAsc, b1.description, b2.description);
                case Column::duration:
                    return orderPred(isAsc, b1.duration, b2.duration);
                case Column::name:
                    return orderPred(isAsc, b1.name, b2.name);
                case Column::startTime:
                    return orderPred(isAsc, b1.startTimestamp, b2.startTimestamp);
                case Column::tags:
                {
                    if (b1.tags.size() != b2.tags.size())
                        return orderPred(isAsc, b1.tags.size(), b2.tags.size());

                    for (size_t i = 0; i < b1.tags.size(); ++i)
                    {
                        if (b1.tags[i] != b2.tags[i])
                            return orderPred(isAsc, b1.tags[i], b2.tags[i]);
                    }

                    return false;
                }
            }

            return false;
        });

    if (filter.limit && filter.limit < outBookmarks->size())
        outBookmarks->resize(*filter.limit);
}

bool motionMaches(const Motion& motion, const MotionFilter& filter)
{
    if (!filter.deviceIds.empty())
    {
        const bool deviceMatches = std::any_of(
            filter.deviceIds.cbegin(), filter.deviceIds.cend(),
            [&motion](const auto& id) { return id == motion.deviceId; });

        if (!deviceMatches)
            return false;
    }

    if (!filter.timePeriod.contains(motion.startTimestamp))
        return false;

    const bool isEmptyFilter = std::all_of(
        filter.regions.begin(), filter.regions.end(),
        [](const auto& rect)
        {
            return rect.isEmpty();
        });

    if (isEmptyFilter)
        return true;

    // Channel starts from 0 and is incremented by 1.
    if (motion.channel < filter.regions.size())
    {
        auto rect = filter.regions[motion.channel];
        if (rect.isEmpty())
            rect = Rect{0.f, 0.f, (double) kMotionGridWidth, (double) kMotionGridHeight};

        auto matcher = RegionMatcher(rect);
        if (matcher.match(fromBase64(motion.dataBase64)))
            return true;
    }

    return false;
}

bool objectTrackMatches(const ObjectTrack& objectTrack, const AnalyticsFilter& filter)
{
    if (!filter.deviceIds.empty())
    {
        const bool deviceIdMatches = std::any_of(
            filter.deviceIds.cbegin(), filter.deviceIds.cend(),
            [&objectTrack](const auto& id) { return id == objectTrack.deviceId; });

        if (!deviceIdMatches)
            return false;
    }

    if (!filter.objectTypeIds.empty())
    {
        const bool objectTypeIdMatches = std::any_of(
            filter.objectTypeIds.cbegin(), filter.objectTypeIds.cend(),
            [&objectTrack](const auto& filterTypeId)
            {
                return objectTrack.objectTypeId.find(filterTypeId) != std::string::npos;
            });

        if (!objectTypeIdMatches)
            return false;
    }

    if (filter.objectTrackId && filter.objectTrackId != objectTrack.id)
        return false;

    using namespace std::chrono;
    if (!filter.timePeriod.contains(
        duration_cast<milliseconds>(objectTrack.firstAppearanceTimestamp)))
    {
        return false;
    }

    if (filter.boundingBox)
    {
        RegionMatcher regionMatcher(*filter.boundingBox);
        const auto objectTrackRegion =
            fromBase64(objectTrack.objectPosition.boundingBoxGridBase64);

        if (!regionMatcher.match(objectTrackRegion))
            return false;
    }

    if (filter.analyticsEngineId && filter.analyticsEngineId != objectTrack.analyticsEngineId)
        return false;

    if (filter.attributeSearchConditions.empty())
        return true;

    if (objectTrack.attributes.empty())
        return false;

    bool attributeMatch = true;
    for (const auto& searchCondition: filter.attributeSearchConditions)
    {
        switch (searchCondition.type)
        {
            case AttributeSearchCondition::Type::attributePresenceCheck:
                attributeMatch =
                    hasAttributeWithName(searchCondition.name, objectTrack.attributes);
                break;
            case AttributeSearchCondition::Type::attributeValueMatch:
                attributeMatch =
                    hasAttributeWithName(searchCondition.name, objectTrack.attributes);
                if (attributeMatch)
                {
                    attributeMatch =
                        hasAttributeWithValue(searchCondition.value, objectTrack.attributes);
                }
                break;
            case AttributeSearchCondition::Type::numericRangeMatch:
                attributeMatch =
                    hasAttributeWithName(searchCondition.name, objectTrack.attributes);
                if (attributeMatch)
                {
                    attributeMatch =
                        hasAttributeWithRange(searchCondition.range, objectTrack.attributes);
                }
                break;
            case AttributeSearchCondition::Type::textMatch:
                if (!searchCondition.text.empty())
                {
                    bool found = false;
                    for (const auto& a: objectTrack.attributes)
                    {
                        if (a.name.find(searchCondition.text) != std::string::npos
                            || a.value.find(searchCondition.text) != std::string::npos)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        attributeMatch = false;
                }
                break;
        }

        if (!attributeMatch && !searchCondition.isNegative)
            return false;
    }

    return true;
}

TimePeriodList sortAndLimitTimePeriods(
    TimePeriodList periods, SortOrder order, std::optional<int> limit)
{
    if (order != periods.sortOrder())
        periods.reverse();

    if (limit)
        periods.shrink(*limit);

    return periods;
}

std::vector<uint8_t> fromBase64(const std::string& data)
{
    static constexpr int8_t b64[] =
        { 62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,
        10,11, 12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,
        34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51 };

    enum class State { one, two, three, four } state = State::one;
    int8_t c;
    size_t index = 0;
    const auto decodeCh =
        [](char toDecode)
        {
            size_t i = (int)toDecode - 43;
            if (i < 0 || i >= sizeof(b64))
                throw std::logic_error("Not b64 " + std::to_string(i));

            int8_t decoded = b64[i];
            if (decoded < 0)
                throw std::logic_error("Invalid b64 " + std::to_string(i));

            return decoded;
        };

    std::vector<uint8_t> result;
    result.reserve(std::round(data.size() * 0.8f));
    int8_t decoded = 0;
    while (true)
    {
        if (index == data.size() || data[index] == '=')
            break;

        switch (state)
        {
            case State::one:
                c = decodeCh(data[index]) << 0x2;
                state = State::two;
                break;
            case State::two:
                decoded = decodeCh(data[index]);
                c |= decoded >> 0x4;
                result.push_back(c);
                c = (decoded & 0xf) << 4;
                state = State::three;
                break;
            case State::three:
                decoded = decodeCh(data[index]);
                c |= decoded >> 2;
                result.push_back(c);
                c = decoded << 0x6;
                state = State::four;
                break;
            case State::four:
                c |= decodeCh(data[index]);
                result.push_back(c);
                state = State::one;
                break;
        }

        ++index;
    }

    return result;
}

std::string toBase64(const std::vector<uint8_t>& data)
{
    return toBase64(data.data(), data.size());
}

std::string toBase64(const uint8_t* data, int size)
{
    static constexpr const char* const b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    enum class State { one, two, three } state = State::one;
    std::string result;
    result.reserve((size_t) (size * 1.5f));
    char c;
    size_t index = 0;
    while (true)
    {
        if (index == size)
        {
            switch (state)
            {
                case State::one:
                    break;
                case State::two:
                    result.push_back(b64[(size_t) c]);
                    result.push_back('=');
                    result.push_back('=');
                    break;
                case State::three:
                    result.push_back(b64[(size_t) c]);
                    result.push_back('=');
                    break;
            }

            break;
        }

        switch (state)
        {
            case State::one:
                result.push_back(b64[(size_t) ((data[index] & 0xfc) >> 2)]);
                c = ((data[index] & 0x03) << 4) & 0xfc;
                state = State::two;
                break;
            case State::two:
                c |= ((data[index] & 0xf0) >> 4);
                result.push_back(b64[(size_t) c]);
                c = ((data[index] & 0xf) << 2) & 0xfc;
                state = State::three;
                break;
            case State::three:
                c |= ((data[index] & 0xc0) >> 6);
                result.push_back(b64[(size_t) c]);
                result.push_back(b64[(size_t) (data[index] & 0x3f)]);
                state = State::one;
                break;
        }

        ++index;
    }

    return result;
}

nx::sdk::cloud_storage::IDeviceAgent* findDeviceAgentById(
    const std::string& id,
    const std::vector<nx::sdk::Ptr<nx::sdk::cloud_storage::IDeviceAgent>>& devices)
{
    auto resultIt = std::find_if(
        devices.cbegin(), devices.cend(),
        [&id](const auto& devicePtr) { return id == deviceId(devicePtr.get()); });

    if (resultIt == devices.cend())
        return nullptr;

    return resultIt->get();
}

} // namespace nx::sdk::cloud_storage
