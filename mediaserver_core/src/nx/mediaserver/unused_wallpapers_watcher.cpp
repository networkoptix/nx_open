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

    auto fileManager = connection->getStoredFileManager(Qn::kSystemAccess);
    QStringList fileList;
    auto result = fileManager->listDirectorySync(Qn::kWallpapersFolder, &fileList);
    if (result != ec2::ErrorCode::ok)
        return;
    std::sort(fileList.begin(), fileList.end());

    auto layoutManager = connection->getLayoutManager(Qn::kSystemAccess);
    ec2::ApiLayoutDataList layoutList;
    result = layoutManager->getLayoutsSync(&layoutList);
    if (result != ec2::ErrorCode::ok)
        return;
    QSet<QString> usingNames;
    for (const auto& value : layoutList)
        usingNames.insert(value.backgroundImageFilename);

    fileList.erase(std::remove_if(fileList.begin(), fileList.end(),
        [&usingNames](const QString& fileName)
        {
            return usingNames.contains(fileName);
        }), fileList.end());

    QStringList intersection;
    std::set_intersection(
        fileList.begin(), fileList.end(),
        m_previousData.begin(), m_previousData.end(),
        std::inserter(intersection, intersection.begin()));

    auto manager = commonModule()->ec2Connection()->getStoredFileManager(Qn::kSystemAccess);
    // Remove unused files from database if no layout have been found for some period of time.
    for (const auto& fileName: intersection)
    {
        NX_INFO(
            this,
            lm("Automatically remove Db file %1 because it is not used on layouts any more.")
            .arg(fileName));
        manager->deleteStoredFile(
            Qn::kWallpapersFolder + L'/' + fileName,
            ec2::DummyHandler::instance(),
            &ec2::DummyHandler::onRequestDone);
    }

    m_previousData = fileList;
}

} // namespace mediaserver
} // namespace nx
