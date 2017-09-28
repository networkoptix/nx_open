#include <vector>
#include <memory>

#include <gtest/gtest.h>
#include <test_support/mediaserver_launcher.h>
#include <nx/mediaserver/unused_wallpapers_watcher.h>
#include "nx_ec/ec_api.h"

namespace nx {
namespace mediaserver {
namespace test {

QStringList readFiles(const std::shared_ptr<ec2::AbstractECConnection>& ec2Connection)
{
    QStringList outputData;
    auto fileManager = ec2Connection->getStoredFileManager(Qn::kSystemAccess);
    fileManager->listDirectorySync(Qn::kWallpapersFolder, &outputData);
    return outputData;
}

TEST(UnusedLayoutsWatcherTest, main)
{
    std::unique_ptr<MediaServerLauncher> mediaServerLauncher;
    mediaServerLauncher.reset(new MediaServerLauncher());
    ASSERT_TRUE(mediaServerLauncher->start());

    auto ec2Connection = mediaServerLauncher->commonModule()->ec2Connection();

    ec2::ApiLayoutData apiLayout;
    apiLayout.id = QnUuid::createUuid();
    apiLayout.backgroundImageFilename = lit("file1.jpg");

    auto layoutManager = ec2Connection->getLayoutManager(Qn::kSystemAccess);
    ASSERT_EQ(ec2::ErrorCode::ok, layoutManager->saveSync(apiLayout));

    auto fileManager = ec2Connection->getStoredFileManager(Qn::kSystemAccess);
    fileManager->addStoredFileSync(lit("%1/file1.jpg").arg(Qn::kWallpapersFolder), QByteArray());
    fileManager->addStoredFileSync(lit("%1/file2.jpg").arg(Qn::kWallpapersFolder), QByteArray());

    ASSERT_EQ(2, readFiles(ec2Connection).size());
    nx::mediaserver::UnusedWallpapersWatcher fileDeleter(mediaServerLauncher->commonModule());

    fileDeleter.update();
    ASSERT_EQ(2, readFiles(ec2Connection).size());

    fileDeleter.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(1, readFiles(ec2Connection).size());

    fileDeleter.update();
    ASSERT_EQ(1, readFiles(ec2Connection).size());
}

} // namespace test
} // namespace mediaserver
} // namespace nx
