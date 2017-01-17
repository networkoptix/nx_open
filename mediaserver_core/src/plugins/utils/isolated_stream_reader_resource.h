#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

namespace nx {
namespace plugins {
namespace utils {

class IsolatedStreamReaderResource: public QnPlOnvifResource
{

public:
    virtual bool hasProperty(const QString &key) const override;
    virtual QString getProperty(const QString &key) const override;

    virtual bool setProperty(
        const QString &key,
        const QString &value,
        PropertyOptions options = DEFAULT_OPTIONS) override;

private:
    mutable QnMutex m_propertyMutex;
    std::map<QString, QString> m_properties;
};

} // namespace utils
} // namespace plugins
} // namespace nx

#endif // ENABLE_ONVIF
