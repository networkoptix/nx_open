#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <common/common_module.h>
#include <common/static_common_module.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/storage/file_storage/qtfile_storage_resource.h>
#include <nx/core/access/access_types.h>
#include <nx/kit/output_redirector.h>
#include <nx/utils/app_info.h>
#include <utils/media/ffmpeg_initializer.h>
#include <nx/vms/testcamera/test_camera_ini.h>
#include <nx/network/socket_global.h>

#include "cli_options.h"
#include "camera_pool.h"
#include "camera_options.h"
#include "file_cache.h"

namespace nx::vms::testcamera {

namespace {

template<typename... Args>
void print(std::ostream& stream, const QString& message, Args... args)
{
    stream << nx::utils::log::makeMessage(message + "\n", args...).toStdString();
}

static CameraOptions makeCameraOptions(
    const CliOptions& options, const CliOptions::CameraSet& cameraSet)
{
    CameraOptions result;

    result.includePts = options.includePts;
    result.unloopPts = options.unloopPts;
    result.shiftPtsPrimaryPeriodUs = options.shiftPtsPrimaryPeriodUs;
    result.shiftPtsSecondaryPeriodUs = options.shiftPtsSecondaryPeriodUs;

    result.offlineFreq = cameraSet.offlineFreq;

    return result;
}

class Executor
{
public:
    /** On error, prints the error message on stderr and returns false. */
    bool initialize(int argc, char* argv[])
    {
        CliOptions options;
        if (!parseCliOptions(argc, argv, &options))
            return false;

        nx::utils::log::mainLogger()->setDefaultLevel(options.logLevel);

        m_staticCommonModule = std::make_unique<QnStaticCommonModule>(
            nx::vms::api::PeerType::notDefined);

        m_ffmpegInitializer = std::make_unique<QnFfmpegInitializer>();

        m_commonModule = std::make_unique<QnCommonModule>(
            /*clientMode*/ false, nx::core::access::Mode::direct);

        m_commonModule->storagePluginFactory()->registerStoragePlugin(
            "file", QnQtFileStorageResource::instance, /*isDefaultProtocol*/ true);

        m_fileCache = std::make_unique<FileCache>(
            m_commonModule.get(), options.maxFileSizeMegabytes);

        m_cameraPool = std::make_unique<CameraPool>(
            m_fileCache.get(),
            options.localInterfaces,
            m_commonModule.get(),
            options.noSecondary,
            options.fpsPrimary,
            options.fpsSecondary);

        if (!loadFilesAndAddCameras(options))
        {
            print(std::cerr, "Unable to load files or add cameras; see the logged error.");
            return false;
        }

        // NOTE: Printing blank lines before and after to emphasize.
        print(std::cout, "\nLoaded %2 video file(s). Starting %1 camera(s)...\n",
            m_cameraPool->cameraCount(), m_fileCache->fileCount());

        return m_cameraPool->startDiscovery();
    }

private:
    std::unique_ptr<QnStaticCommonModule> m_staticCommonModule;
    std::unique_ptr<QnFfmpegInitializer> m_ffmpegInitializer;
    std::unique_ptr<QnCommonModule> m_commonModule;
    std::unique_ptr<FileCache> m_fileCache;
    std::unique_ptr<CameraPool> m_cameraPool;

private:
    bool loadFilesAndAddCameras(const CliOptions& options) const
    {
        for (const auto& cameraSet: options.cameraSets)
        {
            for (const auto& filename: cameraSet.primaryFileNames)
            {
                if (!m_fileCache->loadFile(filename))
                    return false;
            }
            for (const auto& filename: cameraSet.secondaryFileNames)
            {
                if (!m_fileCache->loadFile(filename))
                    return false;
            }

            const QStringList& secondaryFileNames = cameraSet.secondaryFileNames.isEmpty()
                ? cameraSet.primaryFileNames
                : cameraSet.secondaryFileNames;

            if (!m_cameraPool->addCameraSet(
                m_fileCache.get(),
                cameraSet.videoLayout,
                options.cameraForFile,
                makeCameraOptions(options, cameraSet),
                cameraSet.count,
                cameraSet.primaryFileNames,
                secondaryFileNames))
            {
                return false;
            }
        }
        return true;
    }
};

} // namespace

} using namespace nx::vms::testcamera;

int main(int argc, char* argv[])
{
    try
    {
        nx::kit::OutputRedirector::ensureOutputRedirection();

        ini().reload(); //< Make .ini appear on the console even when help is requested.

        QCoreApplication::setOrganizationName(nx::utils::AppInfo::organizationName());
        QCoreApplication::setApplicationName(nx::utils::AppInfo::vmsName() + " Test Camera");
        QCoreApplication::setApplicationVersion(nx::utils::AppInfo::applicationVersion());
        QCoreApplication app(argc, argv); //< Each user may have their own testcamera running.

        // NOTE: Print blank lines before (after ini-config printout) and after to emphasize.
        print(std::cout, "\n%1 version %2\n", app.applicationName(), app.applicationVersion());

        Executor executor;
        if (!executor.initialize(argc, argv))
            return 1;

        const int exitStatus = app.exec();

        print(std::cerr, "Finished with exit status %1.", exitStatus);
        return exitStatus;
    }
    catch (const std::exception& e)
    {
        print(std::cerr, "INTERNAL ERROR: Exception raised: %1", e.what());
        return 70;
    }
    catch (...)
    {
        print(std::cerr, "INTERNAL ERROR: Unknown exception raised.");
        return 70;
    }
}
