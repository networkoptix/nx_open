#pragma once

#include <plugins/resource/onvif/onvif_resource.h>

namespace nx {
namespace plugins {
namespace pelco {

class OpteraStreamReaderResource: public QnPlOnvifResource
{

public:
    virtual bool hasProperty(const QString &key) const override;
    virtual QString getProperty(const QString &key) const override;

    virtual bool setProperty(
        const QString &key,
        const QString &value,
        PropertyOptions options = DEFAULT_OPTIONS) override;

    virtual bool removeProperty(const QString& key) override;

private:
    mutable QnMutex m_propertyMutex;
    std::map<QString, QString> m_properties;
};

} // namespace pelco
} // namespace plugins
} // namespace nx