#include "unused_wallpapers_watcher.h"

#include <common/common_module.h>
#include <nx_ec/dummy_handler.h>
#include "ec2_connection.h"

namespace nx {
namespace mediaserver {

static const int kUpdateIntervalMs = 1000 * 60 * 30;

UnusedWallpapersWatcher::UnusedWallpapersWatcher(QnCommonModule* commonModule):
    base_type(commonModule)
{
    connect(&m_timer, &QTimer::timeout, this, &UnusedWallpapersWatcher::update);
}

void UnusedWallpapersWatcher::start()
{
    update();
    m_timer.start(kUpdateIntervalMs);
}

void UnusedWallpapersWatcher::update()
{
    auto connectionPtr = commonModule()->ec2Connection();
    if (!connectionPtr)
        return;
    auto connection = dynamic_cast<ec2::Ec2DirectConnection*> (connectionPtr.get());
    static const QString prefix = QString(Qn::wallpapersFolder) + L'/';
    QString queryStr = R"(
        select path from vms_storedFiles
        where path like '%1%'
        and not exists (select 1 from vms_layout where background_image_filename = substr(path, %2))
        order by path
    )";
    queryStr = queryStr.arg(prefix).arg(prefix.length() + 1);

    std::vector<ec2::ApiStoredFilePath> data;
    auto errCode = connection->database()->execSqlQuery(queryStr, &data);
    if (errCode != ec2::ErrorCode::ok)
        return;

    std::vector<ec2::ApiStoredFilePath> intersection;
    std::set_intersection(
        data.begin(), data.end(),
        m_previousData.begin(), m_previousData.end(),
        std::inserter(intersection, intersection.begin()));

    auto manager = commonModule()->ec2Connection()->getStoredFileManager(Qn::kSystemAccess);
    for (const auto& record : intersection)
    {
        NX_INFO(
            this,
            lm("Automatically remove Db file %1 because it is not used on layouts any more.")
            .arg(record.path));
        manager->deleteStoredFile(
            record.path,
            ec2::DummyHandler::instance(),
            &ec2::DummyHandler::onRequestDone);
    }

    m_previousData = data;
}

} // namespace mediaserver
} // namespace nx
