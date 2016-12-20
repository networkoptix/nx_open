#ifdef ENABLE_ONVIF

#include "optera_stream_reader_resource.h"

namespace nx {
namespace plugins {
namespace pelco {

bool OpteraStreamReaderResource::hasProperty(const QString &key) const
{
    QnMutexLocker lock(&m_propertyMutex);
    return m_properties.find(key) != m_properties.end();
}

QString OpteraStreamReaderResource::getProperty(const QString &key) const
{
    QnMutexLocker lock(&m_propertyMutex);
    auto propertyValue = m_properties.find(key);

    if (propertyValue != m_properties.end())
        return propertyValue->second;

    auto resType = qnResTypePool->getResourceType(getTypeId());
    if (resType)
        return resType->defaultValue(key);

    return QString();

}

bool OpteraStreamReaderResource::setProperty(
    const QString &key,
    const QString &value,
    PropertyOptions /*options*/)
{
    QnMutexLocker lock(&m_propertyMutex);
    m_properties[key] = value;
    return true;
}

} // namespace pelco
} // namespace plugins
} // namespace nx

#endif //ENABLE_ONVIF
