#include "add_default_webpages_migration.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <core/resource/resource_type.h>

#include <database/api/db_webpage_api.h>

#include <nx_ec/data/api_webpage_data.h>

#include <nx/utils/log/log.h>

namespace ec2 {
namespace database {
namespace migrations {

bool addDefaultWebpages(ec2::database::api::Context* context)
{
    auto addWebPage = [context](const QString& name, const QString& url)
        {
            NX_ASSERT(!name.isEmpty());
            NX_ASSERT(QUrl(url).isValid());
            if (name.isEmpty() || url.isEmpty() || !QUrl(url).isValid())
                return false;

            // keeping consistency with QnWebPageResource
            ApiWebPageData webPage;
            webPage.id = guidFromArbitraryData(url);
            webPage.typeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId);
            webPage.url = url;
            webPage.name = name;
            return api::saveWebPage(context, webPage);
        };

    QFile config(":/serverProperties.json");
    if (!config.open(QIODevice::ReadOnly))
    {
        NX_LOG(lit("Could not read serverProperties.json"), cl_logWARNING);
        return true; // We don't want to crash if partner did not fill any of these
    }

    QString encoded = config.readAll();
    QJsonObject configContents = QJsonDocument::fromJson(encoded.toUtf8()).object();
    QJsonObject defaultWebPages = configContents.value("defaultWebPages").toObject();
    for (auto iter = defaultWebPages.constBegin(); iter != defaultWebPages.constEnd(); ++iter)
    {
        const QString name = iter.key();
        const QString url = iter->toString();
        bool success = addWebPage(name, url);
        NX_ASSERT(success);
        if (!success)
            NX_LOG(lit("Invalid predefined url %1: %2").arg(name).arg(url), cl_logERROR);
    }

    return true; // We don't want to crash if partner did not fill any of these
}

} // namespace migrations
} // namespace database
} // namespace ec2
