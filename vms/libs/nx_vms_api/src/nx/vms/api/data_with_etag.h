// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <tuple>
#include <vector>

namespace nx::vms::api {

template<typename Data>
struct DataWithEtag: Data
{
    std::optional<std::string> etag;

    using DbReadTypes = std::tuple<Data>;
    using DbUpdateTypes = std::tuple<Data>;
    using DbListTypes = std::tuple<std::vector<Data>>;

    DbUpdateTypes toDbTypes() && { return {std::move(static_cast<Data&>(*this))}; }

    static std::vector<DataWithEtag<Data>> fromDbTypes(DbListTypes dataFromDb)
    {
        std::vector<DataWithEtag<Data>> result;
        auto& list = std::get<0>(dataFromDb);
        result.resize(list.size());
        for (size_t i = 0; i < list.size(); ++i)
            static_cast<Data&>(result[i]) = std::move(list[i]);
        return result;
    }
};

} // namespace nx::vms::api
