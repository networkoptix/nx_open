// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <vector>

#include <QtCore/QString>

namespace nx::network::rest {

class NX_NETWORK_REST_API CollectionHash
{
public:
    using Value = std::string; //< Collection combined hash + possible item hash.
    using ValueView = std::string_view; //< Collection combined hash + possible item hash.
    using ItemId = QString;

    struct Item
    {
        ItemId id;
        std::string raw;
    };

    Value calculate(Item item);
    Value calculate(std::vector<Item> list);
    Value remove(const ItemId& id);

    enum Check { item, list };
    static bool check(Check check, ValueView in, ValueView out);

private:
    using Hash = std::string; //< std::array<char, 16>
    using HashById = std::map<ItemId, Hash>;

private:
    HashById m_hashes;
    Hash m_combinedHash;
};

} // namespace nx::network::rest
