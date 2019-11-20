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
#include <core/resource/test_camera_ini.h>
#include <nx/network/socket_global.h>

#include "cli_options.h"
#include "camera_pool.h"
#include "camera_options.h"
#include "file_cache.h"

namespace nx::vms::testcamera {

namespace {

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
    bool initialize(int argc, char* argv[])
    {
        CliOptions options;
        if (!parseCliOptions(argc, argv, &options))
            return false;

        nx::utils::log::mainLogger()->setDefaultLevel(options.logLevel);

        m_staticCommonModule.reset(new QnStaticCommonModule(nx::vms::api::PeerType::notDefined));

        m_ffmpegInitializer.reset(new QnFfmpegInitializer());

        m_commonModule.reset(new QnCommonModule(
            /*clientMode*/ false, nx::core::access::Mode::direct));

        m_commonModule->storagePluginFactory()->registerStoragePlugin(
            "file", QnQtFileStorageResource::instance, /*isDefaultProtocol*/ true);

        m_fileCache.reset(new FileCache(m_commonModule.get(), options.maxFileSizeMegabytes));

        m_cameraPool.reset(new CameraPool(
            options.localInterfaces,
            m_commonModule.get(),
            options.noSecondary,
            options.fpsPrimary,
            options.fpsSecondary));

        if (!loadFilesAndAddCameras(options))
            return false;

        std::cout << lm("\nLoaded %2 video file(s). Starting %1 camera(s)...\n\n")
            .args(m_cameraPool->cameraCount(), m_fileCache->fileCount()).toStdString();

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

            m_cameraPool->addCameras(
                m_fileCache.get(),
                options.cameraForFile,
                makeCameraOptions(options, cameraSet),
                cameraSet.count,
                cameraSet.primaryFileNames,
                secondaryFileNames);
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

        testCameraIni().reload(); //< Make .ini appear on the console even when help is requested.

        QCoreApplication::setOrganizationName(nx::utils::AppInfo::organizationName());
        QCoreApplication::setApplicationName(nx::utils::AppInfo::vmsName() + " Test Camera");
        QCoreApplication::setApplicationVersion(nx::utils::AppInfo::applicationVersion());

        // Each user may have their own testcamera running.
        QCoreApplication app(argc, argv);

        std::cout << (
            "\n" //< Blank line after the ini-config printout.
            + app.applicationName() + " version " + app.applicationVersion() + "\n\n"
        ).toStdString();

        Executor executor;
        if (!executor.initialize(argc, argv))
            return 1;

        const int exitStatus = app.exec();

        std::cerr << "Finished with exit status " << exitStatus << ".\n";
        return exitStatus;
    }
    catch (const std::exception& e)
    {
        std::cerr << "INTERNAL ERROR: Exception raised: " << e.what() << "\n";
        return 70;
    }
    catch (...)
    {
        std::cerr << "INTERNAL ERROR: Unknown exception raised.\n";
        return 70;
    }
}
