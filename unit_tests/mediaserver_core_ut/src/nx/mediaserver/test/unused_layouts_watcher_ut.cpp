#include <vector>
#include <memory>

#include <gtest/gtest.h>
#include "mediaserver_launcher.h"
#include <nx/mediaserver/unused_wallpapers_watcher.h>
#include "nx_ec/ec_api.h"

namespace nx {
namespace mediaserver {
namespace test {

class UnusedLayoutsWatcherTest:
    public QObject,
    public ::testing::Test
{
private:
    std::unique_ptr<MediaServerLauncher> mediaServerLauncher;
public:

    QStringList readFiles(const std::shared_ptr<ec2::AbstractECConnection>& ec2Connection)
    {
        QStringList outputData;
        auto fileManager = ec2Connection->getStoredFileManager(Qn::kSystemAccess);
        fileManager->listDirectorySync(Qn::wallpapersFolder, &outputData);
        return outputData;
    }

    void testMain()
    {
        mediaServerLauncher.reset(new MediaServerLauncher());
        ASSERT_TRUE(mediaServerLauncher->start());

        auto ec2Connection = mediaServerLauncher->commonModule()->ec2Connection();

        ec2::ApiLayoutData apiLayout;
        apiLayout.id = QnUuid::createUuid();
        apiLayout.backgroundImageFilename = lit("file1.jpg");

        auto layoutManager = ec2Connection->getLayoutManager(Qn::kSystemAccess);
        ASSERT_EQ(ec2::ErrorCode::ok, layoutManager->saveSync(apiLayout));

        auto fileManager = ec2Connection->getStoredFileManager(Qn::kSystemAccess);
        fileManager->addStoredFileSync(lit("%1/file1.jpg").arg(Qn::wallpapersFolder), QByteArray());
        fileManager->addStoredFileSync(lit("%1/file2.jpg").arg(Qn::wallpapersFolder), QByteArray());

        ASSERT_EQ(2, readFiles(ec2Connection).size());
        nx::mediaserver::UnusedWallpapersWatcher fileDeleter(mediaServerLauncher->commonModule());

        fileDeleter.update();
        ASSERT_EQ(2, readFiles(ec2Connection).size());

        fileDeleter.update();
        ASSERT_EQ(1, readFiles(ec2Connection).size());

        fileDeleter.update();
        ASSERT_EQ(1, readFiles(ec2Connection).size());
    }
};

TEST_F(UnusedLayoutsWatcherTest, main)
{
    testMain();
}

} // namespace test
} // namespace mediaserver
} // namespace nx
