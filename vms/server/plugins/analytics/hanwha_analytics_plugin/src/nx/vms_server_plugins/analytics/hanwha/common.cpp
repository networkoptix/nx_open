#include "common.h"

#include <QtCore/QRegExp>

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::vms_server_plugins::analytics::hanwha::Hanwha,
    EventItemType)

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

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

QString Hanwha::EngineManifest::eventTypeIdByName(const QString& eventName) const
{
    QString result = m_eventTypeIdByInternalName.value(eventName);
    if (!result.isEmpty())
        return result;

    for (const auto& eventTypeDescriptor: eventTypes)
    {
        if (doesMatch(eventName, eventTypeDescriptor.internalName))
        {
            m_eventTypeIdByInternalName.insert(eventName, eventTypeDescriptor.id);
            return eventTypeDescriptor.id;
        }
    }

    return QString();
}

const Hanwha::EventType& Hanwha::EngineManifest::eventTypeDescriptorById(
    const QString& id) const
{
    static const Hanwha::EventType defaultEventTypeDescriptor{};

    auto it = m_eventTypeDescriptorById.find(id);
    if (it != m_eventTypeDescriptorById.end())
        return it.value();
    for (const auto& eventTypeDescriptor: eventTypes)
    {
        if (eventTypeDescriptor.id == id)
        {
            it = m_eventTypeDescriptorById.insert(id, eventTypeDescriptor);
            return it.value();
        }
    }

    return defaultEventTypeDescriptor;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::vms_server_plugins::analytics::hanwha::Hanwha::EventType,
    (json), HanwhaEventType_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::vms_server_plugins::analytics::hanwha::Hanwha::EngineManifest,
    (json), HanwhaEngineManifest_Fields)

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
