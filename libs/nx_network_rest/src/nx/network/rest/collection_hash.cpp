// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "collection_hash.h"

#include <nx/utils/cryptographic_hash.h>

namespace nx::network::rest {

namespace {

using Hash = std::string; //< std::array<char, 16>

static constexpr auto kHashSize = 16;
static const auto kEmptyHash = nx::utils::sha3_256(std::string_view{}).substr(0, kHashSize);

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

} // namespace

CollectionHash::CollectionHash(std::vector<Item> list)
{
    calculate(std::move(list));
}

std::pair<CollectionHash::Value, bool /*changed*/> CollectionHash::calculate(Item item)
{
    auto hash = calculateHash(std::move(item.raw));
    auto it = m_hashes.find(item.id);
    if (it != m_hashes.end())
    {
        if (it->second == hash)
            return {m_combinedHash + std::move(hash), /*changed*/ false};

        it->second = std::move(hash);
    }
    else
    {
        it = m_hashes.emplace(std::move(item.id), std::move(hash)).first;
    }
    return {(m_combinedHash = combineHashes(m_hashes)) + it->second, /*changed*/ true};
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

bool CollectionHash::check(Check check, ValueView in, ValueView out)
{
    if (in.size() != out.size())
        check = list;
    switch (check)
    {
        case item:
            if (in.size() == kHashSize * 2)
                in = in.substr(kHashSize, kHashSize);
            if (out.size() == kHashSize * 2)
                out = out.substr(kHashSize, kHashSize);
            break;
        case list:
            if (in.size() == kHashSize * 2)
                in = in.substr(0, kHashSize);
            if (out.size() == kHashSize * 2)
                out = out.substr(0, kHashSize);
            break;
    }
    return in == out;
}

} // namespace nx::network::rest
