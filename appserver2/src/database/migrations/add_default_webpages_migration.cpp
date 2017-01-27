#include "add_default_webpages_migration.h"

#include <QtCore/QUrl>

#include <core/resource/resource_type.h>
#include <core/resource/webpage_resource.h>

#include <database/api/db_webpage_api.h>

#include <nx_ec/data/api_webpage_data.h>

#include <utils/common/app_info.h>

#include <nx/utils/log/log.h>

namespace ec2 {
namespace database {
namespace migrations {

bool addDefaultWebpages(const QSqlDatabase& database)
{
    auto addWebPage = [&database](const QString& target)
        {
            QUrl url(target);
            if (!url.isValid() || url.host().isEmpty())
                return true;

            // keeping consistency with QnWebPageResource
            ApiWebPageData webPage;
            webPage.id = guidFromArbitraryData(url.toString().toUtf8());
            webPage.typeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId);
            webPage.url = target;
            webPage.name = QnWebPageResource::nameForUrl(url);
            return api::saveWebPage(database, webPage);
        };

    QSet<QString> urls;
    urls.insert(QnAppInfo::companyUrl().trimmed());
    urls.insert(QnAppInfo::supportLink().trimmed());

    for (const auto& url: urls)
    {
        if (url.isEmpty())
            continue;

        bool success = addWebPage(url);
        NX_ASSERT(success);
        if (!success)
            NX_LOG(lit("Invalid predefined url %1").arg(url), cl_logERROR);
    }

    return true; // We don't want to crash if partner did not fill any of these
}

} // namespace migrations
} // namespace database
} // namespace ec2
