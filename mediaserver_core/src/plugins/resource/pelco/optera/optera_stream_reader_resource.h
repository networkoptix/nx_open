#pragma once

#include <plugins/resource/onvif/onvif_resource.h>

namespace nx {
namespace plugins {
namespace pelco {

class OpteraStreamReaderResource: public QnPlOnvifResource
{

public:
    virtual bool hasProperty(const QString &key) const;
    virtual QString getProperty(const QString &key) const;

    virtual bool setProperty(
        const QString &key,
        const QString &value,
        PropertyOptions options = DEFAULT_OPTIONS);

    virtual bool removeProperty(const QString& key);

private:
    mutable QnMutex m_propertyMutex;
    std::map<QString, QString> m_properties;
};

} // namespace pelco
} // namespace plugins
} // namespace nx