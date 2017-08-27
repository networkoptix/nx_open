#pragma once

#include <QtCore/QString>
#include <QtCore/QFlags>

#include <unordered_map>

#include <boost/optional/optional.hpp>

#include <hanwha_common.h>
#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class HanwhaStringHelper
{
public:
    enum class TemplateFlag
    {
        stateDependent = 0x1,
        regionDependent = 0x2,
        itemDependent = 0x4
    };

    Q_DECLARE_FLAGS(TemplateFlags, TemplateFlag);

    static QString buildCaption(
        const nxpl::NX_GUID& eventType,
        boost::optional<int> eventChannel,
        boost::optional<int> eventRegion,
        HanwhaEventItemType eventItemType,
        bool isActive);

    static QString buildDescription(
        const nxpl::NX_GUID& eventType,
        boost::optional<int> eventChannel,
        boost::optional<int> eventRegion,
        HanwhaEventItemType eventItemType,
        bool isActive);

    static boost::optional<nxpl::NX_GUID> fromStringToEventType(const QString& eventName);

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

    static boost::optional<QString> descriptionTemplate(const nxpl::NX_GUID& eventType);
    static boost::optional<QString> stateStringTemplate(const nxpl::NX_GUID& eventType, bool isActive);
    static boost::optional<QString> regionStringTemplate(const nxpl::NX_GUID& eventType, bool eventState);
    static QString itemStringTemplate(HanwhaEventItemType eventItemType);

    static TemplateFlags templateFlags(const nxpl::NX_GUID& eventType);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HanwhaStringHelper::TemplateFlags);

} // namespace nx
} // namespace mediaserver
} // namespace plugins
