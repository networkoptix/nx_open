#include "add_default_webpages_migration.h"

#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <core/resource/resource_type.h>

#include <database/api/db_webpage_api.h>

#include <nx/vms/api/data/webpage_data.h>

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>
#include <nx/utils/app_info.h>

namespace ec2 {
namespace database {
namespace migrations {

struct FunctionsTag{};

void addWebPage(ec2::database::api::QueryContext* context, const QString& name, const QString& url)
{
    // Unfilled page is definitely ok.
    if (url.isEmpty())
        return;

    if (!NX_ASSERT(nx::utils::Url(url).isValid()))
        return;

    // keeping consistency with QnWebPageResource
    nx::vms::api::WebPageData webPage;
    webPage.id = guidFromArbitraryData(url);
    webPage.url = url;
    webPage.name = name;
    if (!api::saveWebPage(context, webPage))
        NX_ERROR(typeid(FunctionsTag), "Error while saving predefined url %1", url);
};

bool addDefaultWebpages(ec2::database::api::QueryContext* context)
{
    addWebPage(context, "Home Page", nx::utils::AppInfo::homePage());
    addWebPage(context, "Support", nx::utils::AppInfo::supportPage());

    // We don't want to crash if partner did not fill any of these.
    return true;
}

} // namespace migrations
} // namespace database
} // namespace ec2
