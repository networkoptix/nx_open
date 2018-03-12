#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
namespace plugins {

QnMutex Hikvision::DriverManifest::m_cachedIdMutex;
QMap<QString, QnUuid> Hikvision::DriverManifest::m_idByInternalName;
QMap<QnUuid, Hikvision::EventDescriptor> Hikvision::DriverManifest::m_recordById;

QnUuid Hikvision::DriverManifest::eventTypeByInternalName(const QString& value) const
{
    const auto internalEventName = value.toLower();
    QnMutexLocker lock(&m_cachedIdMutex);
    QnUuid result = m_idByInternalName.value(internalEventName);
    if (!result.isNull())
        return result;

    for (const auto& eventDescriptor: outputEventTypes)
    {
        const auto possibleInternalNames = eventDescriptor.internalName.toLower().split(L',');
        for (const auto& name: possibleInternalNames)
        {
            if (internalEventName.contains(name))
            {
                m_idByInternalName.insert(internalEventName, eventDescriptor.typeId);
                return eventDescriptor.typeId;
            }
        }
    }

    return QUuid();
}

const Hikvision::EventDescriptor& Hikvision::DriverManifest::eventDescriptorById(const QnUuid& id) const
{
    QnMutexLocker lock(&m_cachedIdMutex);
    auto itr = m_recordById.find(id);
    if (itr != m_recordById.end())
        return itr.value();
    for (const auto& eventDescriptor: outputEventTypes)
    {
        if (eventDescriptor.typeId == id)
        {
            itr = m_recordById.insert(id, eventDescriptor);
            return itr.value();
        }
    }

    static const Hikvision::EventDescriptor kEmptyDescriptor;
    return kEmptyDescriptor;
}

const Hikvision::EventDescriptor Hikvision::DriverManifest::eventDescriptorByInternalName(const QString& internalName) const
{
    return eventDescriptorById(eventTypeByInternalName(internalName));
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::EventDescriptor, (json), EventDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::DriverManifest, (json), DriverManifest_Fields)

} // plugins
} // mediaserver
} // nx
