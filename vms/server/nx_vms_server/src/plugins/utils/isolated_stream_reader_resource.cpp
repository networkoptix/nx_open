#ifdef ENABLE_ONVIF

#include "isolated_stream_reader_resource.h"

namespace nx {
namespace plugins {
namespace utils {

IsolatedStreamReaderResource::IsolatedStreamReaderResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule)
{
}

bool IsolatedStreamReaderResource::hasProperty(const QString &key) const
{
    QnMutexLocker lock(&m_propertyMutex);
    return m_properties.find(key) != m_properties.end();
}

QString IsolatedStreamReaderResource::getProperty(const QString &key) const
{
    {
        QnMutexLocker lock(&m_propertyMutex);
        auto propertyValue = m_properties.find(key);

        if (propertyValue != m_properties.end())
            return propertyValue->second;
    }

    auto resType = qnResTypePool->getResourceType(getTypeId());
    if (resType)
        return resType->defaultValue(key);

    return QString();

}

bool IsolatedStreamReaderResource::setProperty(
    const QString &key,
    const QString &value,
    PropertyOptions /*options*/)
{
    QnMutexLocker lock(&m_propertyMutex);
    m_properties[key] = value;
    return true;
}

bool IsolatedStreamReaderResource::saveProperties()
{
    // Do nothing.
    return true;
}

int IsolatedStreamReaderResource::savePropertiesAsync()
{
    // Do nothing.
    return 0;
}

int IsolatedStreamReaderResource::saveAsync()
{
    // Do nothing.
    return 0;
}

} // namespace utils
} // namespace plugins
} // namespace nx

#endif //ENABLE_ONVIF
