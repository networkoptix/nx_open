// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "collection_hash.h"

#include <nx/utils/cryptographic_hash.h>

namespace nx::network::rest {

namespace {

static constexpr auto kHashSize = 12;
static const auto kEmptyHash = nx::utils::sha3_256(std::string_view{}).substr(0, kHashSize);

using Hash = std::string; //< std::array<char, kHashSize>

static Hash calculateHash(std::string_view raw)
{
    return raw.empty() ? kEmptyHash : nx::utils::sha3_256(std::move(raw)).substr(0, kHashSize);
}

static Hash combineHashes(const std::map<CollectionHash::ItemId, Hash>& hashes)
{
    std::string join;
    join.reserve(hashes.size() * kHashSize);
    for (const auto& [_, hash]: hashes)
        join += hash;
    return calculateHash(join);
}

static CollectionHash::ValueView itemHash(CollectionHash::ValueView value)
{
    return value.size() == kHashSize * 2 ? value.substr(kHashSize, kHashSize) : value;
}

static CollectionHash::ValueView listHash(CollectionHash::ValueView value)
{
    return value.size() == kHashSize * 2 ? value.substr(0, kHashSize) : value;
}

} // namespace

CollectionHash::CollectionHash(std::vector<Item> list)
{
    calculate(std::move(list));
}

std::pair<CollectionHash::Value, bool /*changed*/> CollectionHash::update(ItemId id, Value hash_)
{
    Hash hash{itemHash(hash_)};
    auto it = m_hashes.find(id);
    if (it != m_hashes.end())
    {
        if (it->second == hash)
            return {m_combinedHash + std::move(hash), /*changed*/ false};

        it->second = std::move(hash);
    }
    else
    {
        it = m_hashes.emplace(std::move(id), std::move(hash)).first;
    }
    return {(m_combinedHash = combineHashes(m_hashes)) + it->second, /*changed*/ true};
}

std::pair<CollectionHash::Value, bool /*changed*/> CollectionHash::calculate(Item item)
{
    return update(std::move(item.id), calculateHash(std::move(item.raw)));
}

CollectionHash::Value CollectionHash::calculate(std::vector<Item> list)
{
    if (list.empty())
    {
        m_hashes.clear();
        return m_combinedHash = kEmptyHash;
    }

    HashById hashes;
    for (auto& item: list)
        hashes.emplace(std::move(item.id), calculateHash(std::move(item.raw)));
    auto combinedHash = combineHashes(hashes);
    m_hashes = std::move(hashes);
    return m_combinedHash = std::move(combinedHash);
}

CollectionHash::Value CollectionHash::remove(const ItemId& id)
{
    if (auto it = m_hashes.find(id); it != m_hashes.end())
    {
        m_hashes.erase(it);
        if (m_hashes.empty())
            return m_combinedHash = kEmptyHash;
        m_combinedHash = combineHashes(m_hashes);
    }
    return m_combinedHash;
}

CollectionHash::Value CollectionHash::hash(const ItemId& id) const
{
    if (auto it = m_hashes.find(id); it != m_hashes.end())
        return m_combinedHash + it->second;
    return m_combinedHash;
}

bool CollectionHash::check(Check check, ValueView in, ValueView out)
{
    if (in.size() != out.size())
        check = list;
    switch (check)
    {
        case item:
            return itemHash(in) == itemHash(out);
        case list:
            return listHash(in) == listHash(out);
    }
    return in == out;
}

} // namespace nx::network::rest
