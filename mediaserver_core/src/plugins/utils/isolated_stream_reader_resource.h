#pragma once

#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

namespace nx {
namespace plugins {
namespace utils {

/**
 * Isolated resource does not put its properties to the database.
 * It can be safely used in places where creation of temporary/proxy resources is needed.
 */

class IsolatedStreamReaderResource: public QnPlOnvifResource
{

public:

    IsolatedStreamReaderResource(QnCommonModule* commonModule);

    virtual bool hasProperty(const QString &key) const override;
    virtual QString getProperty(const QString &key) const override;

    virtual bool setProperty(
        const QString &key,
        const QString &value,
        PropertyOptions options = DEFAULT_OPTIONS) override;

    virtual bool saveParams() override;
    virtual int saveParamsAsync() override;
    virtual int saveAsync() override;
    virtual bool removeProperty(const QString& key) override;

private:
    mutable QnMutex m_propertyMutex;
    std::map<QString, QString> m_properties;
};

} // namespace utils
} // namespace plugins
} // namespace nx

#endif // ENABLE_ONVIF
