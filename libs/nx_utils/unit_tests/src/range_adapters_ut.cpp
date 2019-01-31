#include <gtest/gtest.h>

#include <vector>

#include <QtCore/QMap>

#include <nx/utils/range_adapters.h>

TEST(RangeAdapters, keyValueRange)
{
    QMap<QString, int> source({{"Human", 1}, {"Orc", 2}, {"Elf", 3}});
    QMap<QString, int> destination;

    for (const auto& [name, value]: nx::utils::keyValueRange(source))
        destination.insert(name, value);

    ASSERT_EQ(source, destination);
}

TEST(RangeAdapters, reverseRange)
{
    std::vector<int> source({1, 2, 3, 4, 5});
    std::vector<int> destination;

    for (const auto& value: nx::utils::reverseRange(source))
        destination.push_back(value);

    ASSERT_EQ(destination, std::vector<int>({5, 4, 3, 2, 1}));
}
