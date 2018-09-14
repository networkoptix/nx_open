#include "common.h"

#include <QtCore/QRegExp>

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver_plugins::metadata::hanwha::Hanwha,
    EventItemType)

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
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

QString Hanwha::DriverManifest::eventTypeIdByName(const QString& eventName) const
{
    QString result = m_eventTypeIdByInternalName.value(eventName);
    if (!result.isNull())
        return result;

    for (const auto& eventTypeDescriptor: outputEventTypes)
    {
        if (doesMatch(eventName, eventTypeDescriptor.internalName))
        {
            m_eventTypeIdByInternalName.insert(eventName, eventTypeDescriptor.id);
            return eventTypeDescriptor.id;
        }
    }

    return QString();
}

const Hanwha::EventTypeDescriptor& Hanwha::DriverManifest::eventTypeDescriptorById(
    const QString& id) const
{
    static const Hanwha::EventTypeDescriptor defaultEventTypeDescriptor{};

    auto it = m_eventTypeDescriptorById.find(id);
    if (it != m_eventTypeDescriptorById.end())
        return it.value();
    for (const auto& eventTypeDescriptor: outputEventTypes)
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
    nx::mediaserver_plugins::metadata::hanwha::Hanwha::EventTypeDescriptor,
    (json), EventTypeDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::mediaserver_plugins::metadata::hanwha::Hanwha::DriverManifest,
    (json), DriverManifest_Fields)

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
