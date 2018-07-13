#include "hanwha_common.h"

#include <QtCore/QRegExp>

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver::plugins::Hanwha, EventItemType)

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

bool doesMatch(const QString& realEventName, const QString& internalEventName)
{
    const auto realEventNameString = realEventName.trimmed();
    const auto internalEventNameString = internalEventName.trimmed();

    const auto possibleInternalNames = internalEventNameString.split(
        L',',
        QString::SplitBehavior::SkipEmptyParts);

    for (const auto& name: possibleInternalNames)
    {
        if (realEventNameString.contains(name))
            return true;
    }

    const QRegExp pattern(
        internalEventName,
        Qt::CaseSensitivity::CaseInsensitive,
        QRegExp::PatternSyntax::Wildcard);

    if (!pattern.isValid())
        return false;

    return pattern.exactMatch(realEventName);
}

} // namespace

QnUuid Hanwha::DriverManifest::eventTypeByName(const QString& eventName) const
{
    QnUuid result = m_idByInternalName.value(eventName);
    if (!result.isNull())
        return result;

    for (const auto& eventDescriptor: outputEventTypes)
    {
        if (doesMatch(eventName, eventDescriptor.internalName))
        {
            m_idByInternalName.insert(eventName, eventDescriptor.eventTypeId);
            return eventDescriptor.eventTypeId;
        }
    }

    return QUuid();
}

const Hanwha::EventDescriptor& Hanwha::DriverManifest::eventDescriptorById(const QnUuid& id) const
{
    static const Hanwha::EventDescriptor defaultEventDescriptor{};

    auto itr = m_recordById.find(id);
    if (itr != m_recordById.end())
        return itr.value();
    for (const auto& eventDescriptor: outputEventTypes)
    {
        if (eventDescriptor.eventTypeId == id)
        {
            itr = m_recordById.insert(id, eventDescriptor);
            return itr.value();
        }
    }

    return defaultEventDescriptor;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hanwha::EventDescriptor, (json), EventDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hanwha::DriverManifest, (json), DriverManifest_Fields)

} // plugins
} // mediaserver
} // nx
