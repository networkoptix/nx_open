#pragma once

#include <QtCore/QString>
#include <QtCore/QFlags>

#include <unordered_map>

#include <boost/optional/optional.hpp>

#include <hikvision_common.h>
#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class HikvisionStringHelper
{
public:
    static QString buildCaption(
        const Hikvision::DriverManifest& manifest,
        const QnUuid& eventTypeId,
        boost::optional<int> eventChannel,
        boost::optional<int> eventRegion,
        bool isActive);

    static QString buildDescription(
        const Hikvision::DriverManifest& manifest,
        const QnUuid& eventTypeId,
        boost::optional<int> eventChannel,
        boost::optional<int> eventRegion,
        bool isActive);

private:
    template<typename Key, typename Value>
    static boost::optional<Value> findInMap(
        const Key& key,
        const std::unordered_map<Key, Value>& mapForSearch)
    {
        auto itr = mapForSearch.find(key);
        if (itr != mapForSearch.cend())
            return itr->second;

        return boost::none;
    }
};

} // namespace nx
} // namespace mediaserver
} // namespace plugins

