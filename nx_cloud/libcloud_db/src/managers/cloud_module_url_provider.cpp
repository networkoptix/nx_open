#include "cloud_module_url_provider.h"

#include <QtCore/QFile>

namespace nx {
namespace cdb {

CloudModuleUrlProvider::CloudModuleUrlProvider(const QString& cloudModuleXmlTemplatePath)
{
    loadTemplate(cloudModuleXmlTemplatePath);
}

QByteArray CloudModuleUrlProvider::getCloudModulesXml(QByteArray host) const
{
    auto result = m_cloudModulesXmlTemplate;
    result.replace("%host%", host);
    return result;
}

void CloudModuleUrlProvider::loadTemplate(const QString& cloudModuleXmlTemplatePath)
{
    QFile templateFile(cloudModuleXmlTemplatePath);
    if (!templateFile.open(QIODevice::ReadOnly))
    {
        throw std::runtime_error(
            QString("Failed to open file %1").arg(cloudModuleXmlTemplatePath).toStdString());
    }

    m_cloudModulesXmlTemplate = templateFile.readAll();

    if (m_cloudModulesXmlTemplate.isEmpty())
    {
        throw std::runtime_error(
            QString("File %1 appears to be empty").arg(cloudModuleXmlTemplatePath).toStdString());
    }
}

} // namespace cdb
} // namespace nx
