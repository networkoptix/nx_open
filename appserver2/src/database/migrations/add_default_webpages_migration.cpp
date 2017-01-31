#include "add_default_webpages_migration.h"

#include <QtCore/QUrl>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <core/resource/resource_type.h>

#include <database/api/db_webpage_api.h>

#include <nx_ec/data/api_webpage_data.h>

#include <utils/common/app_info.h>

#include <nx/utils/log/log.h>

namespace ec2 {
namespace database {
namespace migrations {

bool addDefaultWebpages(const QSqlDatabase& database)
{
    auto addWebPage = [&database](const QString& name, const QString& url)
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
            return api::saveWebPage(database, webPage);
        };

    QString encoded = QnAppInfo::defaultWebPages();
    QJsonObject urls = QJsonDocument::fromJson(encoded.toUtf8()).object();
    for (auto iter = urls.constBegin(); iter != urls.constEnd(); ++iter)
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
