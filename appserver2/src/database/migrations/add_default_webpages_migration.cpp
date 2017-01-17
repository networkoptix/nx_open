#include "add_default_webpages_migration.h"

#include <QtCore/QUrl>

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
    auto addWebPage = [&database](const QString& target)
        {
            QUrl url(target);
            if (target.isEmpty() || !url.isValid())
                return true;

            // keeping consistency with QnWebPageResource
            ApiWebPageData webPage;
            webPage.id = guidFromArbitraryData(url.toString().toUtf8());
            webPage.typeId = qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kWebPageTypeId);
            webPage.url = target;
            webPage.name = url.host();
            return api::saveWebPage(database, webPage);
        };

    bool success = addWebPage(QnAppInfo::companyUrl());
    NX_ASSERT(success);
    if (!success)
        NX_LOG(lit("Invalid company url %1").arg(QnAppInfo::companyUrl()), cl_logERROR);

    success = addWebPage(QnAppInfo::supportLink());
    NX_ASSERT(success);
    if (!success)
        NX_LOG(lit("Invalid support link %1").arg(QnAppInfo::supportLink()), cl_logERROR);

    return true; // We don't want to crash if partner did not fill any of these
}

} // namespace migrations
} // namespace database
} // namespace ec2
