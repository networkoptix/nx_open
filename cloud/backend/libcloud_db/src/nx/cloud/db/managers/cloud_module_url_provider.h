#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace nx::cloud::db {

class CloudModuleUrlProvider
{
public:
    /**
     * @throw std::runtime_error On failure to load xml template.
     */
    CloudModuleUrlProvider(const QString& cloudModuleXmlTemplatePath);
    virtual ~CloudModuleUrlProvider() = default;

    QByteArray getCloudModulesXml(QByteArray host) const;

private:
    QByteArray m_cloudModulesXmlTemplate;

    void loadTemplate(const QString& cloudModuleXmlTemplatePath);
};

} // namespace nx::cloud::db
