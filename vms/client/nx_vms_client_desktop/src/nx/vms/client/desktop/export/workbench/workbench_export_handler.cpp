// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_export_handler.h"

#include <QtGui/QAction>
#include <QtWidgets/QPushButton>

#include <client/client_globals.h>
#include <client/client_runtime_settings.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/resource_directory_browser.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/fusion/model_functions.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/utils/grid_walker.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/progress_dialog.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/vms/client/desktop/export/dialogs/export_settings_dialog.h>
#include <nx/vms/client/desktop/export/tools/export_layout_tool.h>
#include <nx/vms/client/desktop/export/tools/export_manager.h>
#include <nx/vms/client/desktop/export/tools/export_media_tool.h>
#include <nx/vms/client/desktop/export/tools/export_media_validator.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/extensions/local_notifications_manager.h>
#include <nx/vms/common/system_settings.h>
#include <platform/environment.h>
#include <recording/time_period.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#ifdef Q_OS_WIN
    #include <launcher/nov_launcher_win.h>
#endif

namespace nx::vms::client::desktop {

using core::LayoutResourcePtr;

namespace {

QnMediaResourceWidget* extractMediaWidget(QnWorkbenchDisplay* display,
    const menu::Parameters& parameters)
{
    if (parameters.size() == 1)
    {
        if (parameters.hasArgument(core::CameraBookmarkRole))
        {
            if (const auto bookmark = parameters.argument<QnCameraBookmark>(core::CameraBookmarkRole);
                bookmark.isValid())
            {
                if (const auto activeWidget = display->activeWidget())
                {
                    if (activeWidget->resource()->getId() == bookmark.cameraId)
                        return dynamic_cast<QnMediaResourceWidget*>(activeWidget);
                }
            }
        }

        return dynamic_cast<QnMediaResourceWidget*>(parameters.widget());
    }

    if ((parameters.size() == 0) && display->widgets().size() == 1)
        return dynamic_cast<QnMediaResourceWidget*>(display->widgets().front());

    return dynamic_cast<QnMediaResourceWidget*>(display->activeWidget());
}

LayoutResourcePtr layoutFromBookmarks(const QnCameraBookmarkList& bookmarks, QnResourcePool* pool)
{
    LayoutResourcePtr layout(new core::LayoutResource());
    layout->setName(WorkbenchExportHandler::tr("%n bookmarks", "", bookmarks.size()));
    layout->setPredefinedCellSpacing(core::CellSpacing::Small);

    // First we must count unique cameras to get the matrix size.
    QnVirtualCameraResourceList cameras;
    QSet<nx::Uuid> placedCameras;
    for (const auto& bookmark: bookmarks)
    {
        if (placedCameras.contains(bookmark.cameraId))
            continue;
        placedCameras.insert(bookmark.cameraId);

        if (const auto& camera = pool->getResourceById<QnVirtualCameraResource>(bookmark.cameraId))
            cameras.push_back(camera);
    }

    const int camerasCount = cameras.size();
    const int matrixSize = std::ceil(std::sqrt(camerasCount));
    NX_ASSERT(matrixSize * matrixSize >= camerasCount);

    const QRect grid(0, 0, matrixSize, matrixSize);
    core::GridWalker gridWalker(grid);

    static const QSize kCellSize(1, 1);

    for (const auto& camera: cameras)
    {
        gridWalker.next();

        nx::vms::common::LayoutItemData item = layoutItemFromResource(camera);
        item.flags = Qn::ItemFlag::Pinned;
        item.combinedGeometry = QRect(gridWalker.pos(), kCellSize);

        layout->addItem(item);
    }

    return layout;
}

core::CachingCameraDataLoaderPtr initDataLoader(const QnVirtualCameraResourcePtr& camera)
{
    const auto systemContext = SystemContext::fromResource(camera);
    if (!NX_ASSERT(systemContext))
        return {};

    const auto loader =
        systemContext->cameraDataManager()->loader(camera, /*createIfNotExists*/ true);
    if (!NX_ASSERT(loader))
        return {};

    auto allowedContent = loader->allowedContent();
    allowedContent.insert(Qn::RecordingContent);
    loader->setAllowedContent(allowedContent);

    return loader;
}

} // namespace

struct WorkbenchExportHandler::Private
{
    Q_DECLARE_TR_FUNCTIONS(WorkbenchExportHandler::Private)

public:
    WorkbenchExportHandler* const q;
    QScopedPointer<ExportManager> exportManager;

    static Filename fileName(const WorkbenchExportHandler::Settings& settings)
    {
        return std::visit([](auto&& settings) { return settings.fileName; }, settings);
    }

    struct ExportContext
    {
        Settings settings;
        QnResourcePtr resource;
        bool saveExistingLayout = false;
        QPointer<ProgressDialog> progressDialog;

        Filename fileName() const { return Private::fileName(settings); }
        nx::core::transcoding::Settings transcodingSettings() const
        {
            return std::visit([](auto&& settings) { return settings.transcodingSettings; }, settings);
        }
    };
    QHash<nx::Uuid, ExportContext> runningExports;

    QScopedPointer<QnMessageBox> startingExportMessageBox;

    explicit Private(WorkbenchExportHandler* owner):
        q(owner),
        exportManager(new ExportManager())
    {
        const auto& manager = q->windowContext()->localNotificationsManager();
        NX_ASSERT(manager);

        connect(manager, &workbench::LocalNotificationsManager::interactionRequested, q,
            [this](const nx::Uuid& exportProcessId)
            {
                if (const auto dialog = runningExports.value(exportProcessId).progressDialog)
                    dialog->show();
            });
    }

    bool isInRunningExports(const Filename& filename) const
    {
        for (const auto& exportContext: runningExports)
        {
            if (exportContext.fileName() == filename)
                return true;
        }
        return false;
    }

    Uuid createExportContext(Settings settings,
        const QnResourcePtr& resource, bool saveExistingLayout)
    {
        const auto& manager = q->windowContext()->localNotificationsManager();
        const QString fullPath = fileName(settings).completeFileName();
        const QString fileNameWithExtension = QFileInfo(fullPath).fileName();

        const auto exportProcessId = manager->addProgress(
            saveExistingLayout ? tr("Saving layout") : tr("Exporting video"), fileNameWithExtension);

        const auto progressDialog = new ProgressDialog(q->mainWindowWidget());
        progressDialog->setText(fullPath);
        progressDialog->setCancelText(saveExistingLayout ? tr("Stop Saving") : tr("Stop Export"));

        if (auto mediaSettings = std::get_if<ExportMediaSettings>(&settings))
        {
            if (mediaSettings->forceVideoTranscoding)
                progressDialog->setInfoText(tr("Video transcoding is required. Export session restarted."));
            else if (mediaSettings->forceAudioTranscoding)
                progressDialog->setInfoText(tr("Audio transcoding is required. Export session restarted."));
        }

        // Adding with ActionRole to make sure button is added to the left of "Cancel".
        auto minimizeButton = new QPushButton(tr("Minimize"), progressDialog);
        progressDialog->addCustomButton(minimizeButton, QDialogButtonBox::ActionRole);
        connect(minimizeButton, &QPushButton::clicked, progressDialog, &QWidget::hide);

        connect(progressDialog, &ProgressDialog::canceled, exportManager.data(),
            [this, exportProcessId]
            {
                q->windowContext()->localNotificationsManager()->remove(exportProcessId);
                exportManager->stopExport(exportProcessId);
            });

        runningExports.insert(exportProcessId,
            ExportContext{settings, resource, saveExistingLayout, progressDialog});

        return exportProcessId;
    }

    std::function<bool(const Filename& filename, bool quiet)> fileNameValidator()
    {
        return
            [this](const Filename& filename, bool quiet) -> bool
            {
                // Most OS do not work well with filenames longer than 255 symbols.
                static const int kFileNameLenghtLimit = 255;

                if (filename.completeFileName().size() > kFileNameLenghtLimit)
                {
                    if (!quiet)
                    {
                        // Second argument must be converted to string to avoid invalid .arg()
                        // overload and to avoid substitutions of possible '%2' in the filename.
                        QnMessageBox::warning(q->mainWindowWidget(), tr("Cannot write file"),
                            tr("%1 filename length is more than %2 characters.", "%1 is file name")
                                .arg(filename.completeFileName(),
                                    QString::number(kFileNameLenghtLimit)));
                    }
                    return false;
                }

                if (isInRunningExports(filename))
                {
                    if (!quiet)
                    {
                        QnMessageBox::warning(q->mainWindowWidget(), tr("Cannot write file"),
                            tr("%1 is in use by another export.", "%1 is file name")
                                .arg(filename.completeFileName()));
                    }
                    return false;
                }

                if (QFileInfo::exists(filename.completeFileName()))
                {
                    if (quiet || !QnFileMessages::confirmOverwrite(
                        q->mainWindowWidget(),
                        filename.completeFileName()))
                    {
                        return false;
                    }
                }

                return true;
            };
    }

    static QnResourcePtr ensureResourceIsInPool(const Filename& filename, QnResourcePool* resourcePool)
    {
        const auto completeFilename = filename.completeFileName();
        NX_ASSERT(QFileInfo(completeFilename).exists());
        if (!QFileInfo(completeFilename).exists())
            return {};

        if (const auto existing = resourcePool->getResourceByUrl(completeFilename))
            return existing;

        switch (filename.extension)
        {
            case FileExtension::avi:
            case FileExtension::mkv:
            case FileExtension::mp4:
            {
                QnAviResourcePtr file(new QnAviResource(completeFilename));
                file->setStatus(nx::vms::api::ResourceStatus::online);
                resourcePool->addResource(file);
                return std::move(file);
            }

            case FileExtension::nov:
            case FileExtension::exe:
            {
                const auto layout = ResourceDirectoryBrowser::layoutFromFile(
                    completeFilename, QPointer(resourcePool));
                if (layout)
                    resourcePool->addResource(layout);
                return layout;
            }
            default:
                NX_ASSERT(false, "Unsupported format");
                return {};
        }
    }
};

WorkbenchExportHandler::WorkbenchExportHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    connect(d->exportManager.get(), &ExportManager::processUpdated, this,
        &WorkbenchExportHandler::exportProcessUpdated);
    connect(d->exportManager.get(), &ExportManager::processFinished, this,
        &WorkbenchExportHandler::exportProcessFinished);

    // Resource directory browser may be not initialized in some scenarios.
    if (auto resourceDirectoryBrowser = appContext()->resourceDirectoryBrowser())
    {
        connect(
            resourceDirectoryBrowser,
            &ResourceDirectoryBrowser::layoutUpdatedFromFile,
            this,
            [](const QnFileLayoutResourcePtr& layout) { layout->storeSnapshot(); });
    }

    connect(action(menu::ExportVideoAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            handleExportVideoAction(parameters);
        });

    connect(action(menu::ExportBookmarkAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            handleExportBookmarkAction(parameters);
        });

    connect(action(menu::ExportBookmarksAction), &QAction::triggered, this,
        &WorkbenchExportHandler::handleExportBookmarksAction);

    connect(action(menu::ExportStandaloneClientAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_exportStandaloneClientAction_triggered);

    // from legacy::WorkbenchExportHandler
    connect(action(menu::SaveLocalLayoutAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_saveLocalLayoutAction_triggered);
}

WorkbenchExportHandler::~WorkbenchExportHandler()
{
    auto runningExports = d->runningExports.keys();
    for (auto key: runningExports)
        d->exportManager->stopExport(key);

    // Try to gracefully wait until export is stopped.
    constexpr auto kStopExportTimeLimitMs = 5000;
    QElapsedTimer timeToStopExports;
    timeToStopExports.start();
    while (!d->runningExports.empty() && !timeToStopExports.hasExpired(kStopExportTimeLimitMs))
        qApp->processEvents();
}

void WorkbenchExportHandler::exportProcessUpdated(const ExportProcessInfo& info)
{
    if (auto dialog = d->runningExports.value(info.id).progressDialog)
    {
        dialog->setRange(info.rangeStart, info.rangeEnd);
        dialog->setValue(info.progressValue);
    }

    windowContext()->localNotificationsManager()->setProgress(info.id,
        qreal(info.progressValue - info.rangeStart) / (info.rangeEnd - info.rangeStart));
}

void WorkbenchExportHandler::exportProcessFinished(const ExportProcessInfo& info)
{
    if (!d->runningExports.contains(info.id))
        return;

    auto exportContext = d->runningExports.take(info.id);
    windowContext()->localNotificationsManager()->remove(info.id);

    d->startingExportMessageBox.reset(); //< Close "Starting Export..." messagebox if exists.

    if (auto dialog = exportContext.progressDialog)
    {
        dialog->hide();
        dialog->deleteLater();
    }

    auto mediaSettings = std::get_if<ExportMediaSettings>(&exportContext.settings);

    NX_VERBOSE(this, "Export finished: %1, status: %2, error: %3",
        exportContext.fileName().name, (int)info.status, ExportProcess::errorString(info.error));

    switch (info.status)
    {
        case ExportProcessStatus::success:
        {
            const auto fileName = exportContext.fileName();
            const auto resource = d->ensureResourceIsInPool(fileName, resourcePool());
            if (const auto layout = resource.dynamicCast<core::LayoutResource>())
                layout->storeSnapshot();

            menu()->trigger(menu::ExportFinishedEvent,
                menu::Parameters().withArgument(
                    Qn::FileNameRole, fileName.completeFileName()));
            if (!exportContext.saveExistingLayout)
                QnMessageBox::success(mainWindowWidget(), tr("Export completed"));
            break;
        }
        case ExportProcessStatus::failure:
        {
            if ((mediaSettings && info.error == ExportProcessError::videoTranscodingRequired
                && !mediaSettings->forceVideoTranscoding)
                || (info.error == ExportProcessError::audioTranscodingRequired
                && !mediaSettings->forceAudioTranscoding))
            {
                if (info.error == ExportProcessError::audioTranscodingRequired)
                    mediaSettings->forceAudioTranscoding = true;
                if (info.error == ExportProcessError::videoTranscodingRequired)
                    mediaSettings->forceVideoTranscoding = true;

                // Run export again with forced transcoding
                auto exportToolInstance = prepareExportTool(
                    exportContext.settings,
                    exportContext.resource,
                    exportContext.saveExistingLayout);
                runExport(std::move(exportToolInstance));
            }
            else
            {
                QnMessageBox::critical(mainWindowWidget(),
                    exportContext.saveExistingLayout ? tr("Saving failed") : tr("Export failed"),
                    ExportProcess::errorString(info.error));
            }
            break;
        }
        case ExportProcessStatus::cancelled:
        {
            break;
        }
        default:
        {
            NX_ASSERT(false, "Invalid state: %1, %2",
                (int)info.status, ExportProcess::errorString(info.error));
        }
    }
}

void WorkbenchExportHandler::handleExportVideoAction(const menu::Parameters& parameters)
{
    // Extracting parameters as fast as we can.
    //const auto parameters = menu()->currentParameters(sender());
    const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    const auto widget = extractMediaWidget(display(), parameters);
    const auto mediaResource = parameters.resource().dynamicCast<QnMediaResource>();
    NX_ASSERT(widget || mediaResource);

    if (!widget && !mediaResource) //< Media resource or widget is mandatory.
        return;

    NX_ASSERT(period.isValid());
    if (!period.isValid())
        return;

    const auto centralResource = widget ? widget->resource() : mediaResource;

    const bool hasPermission = ResourceAccessManager::hasPermissions(
        centralResource,
        Qn::ExportPermission);

    if (!hasPermission && !widget)
        return;

    ExportSettingsDialog dialog(
        period,
        QnCameraBookmark(),
        d->fileNameValidator(),
        context()->watermark(),
        mainWindowWidget());

    if (!hasPermission)
    {
        // We have no permission for camera, but still can view the rest of layout
        static const QString reason =
            tr("Selected period cannot be exported for the current camera.");
        dialog.disableTab(ExportMode::media, reason);
        dialog.setLayout(widget->layoutResource());
    }
    else if (widget)
    {
        const nx::vms::common::LayoutItemData itemData = widget->item()->data();
        // Exporting from the scene and timeline
        dialog.setMediaParams(centralResource, itemData);

        // Why symmetry with the next block is broken?
        const auto periods = parameters.argument<QnTimePeriodList>(Qn::MergedTimePeriodsRole);
        const bool layoutExportAllowed = periods.intersects(period);
        if (layoutExportAllowed)
            dialog.setLayout(widget->layoutResource());
    }
    else
    {
        dialog.setMediaParams(centralResource, nx::vms::common::LayoutItemData());
    }

    if (dialog.exec() != QDialog::Accepted)
        return;

    auto exportToolInstance = prepareExportTool(dialog);
    runExport(std::move(exportToolInstance));
}

void WorkbenchExportHandler::handleExportBookmarkAction(const menu::Parameters& parameters)
{
    const auto bookmark = parameters.argument<QnCameraBookmark>(core::CameraBookmarkRole);
    NX_ASSERT(bookmark.isValid());
    if (!bookmark.isValid())
        return;

    const auto camera =
        resourcePool()->getResourceById<QnVirtualCameraResource>(bookmark.cameraId);
    NX_ASSERT(camera);
    if (!camera)
        return;

    const auto widget = extractMediaWidget(display(), parameters);
    NX_ASSERT(!widget || widget->resource() == camera);

    const bool hasPermission = ResourceAccessManager::hasPermissions(camera, Qn::ExportPermission);
    NX_ASSERT(hasPermission);
    if (!hasPermission)
        return;

    const QnTimePeriod period(bookmark.startTimeMs, bookmark.durationMs);
    ExportSettingsDialog dialog(
        period,
        bookmark,
        d->fileNameValidator(),
        context()->watermark(),
        mainWindowWidget());

    if (const auto loader = initDataLoader(camera))
    {
        connect(
            loader.get(),
            &core::CachingCameraDataLoader::periodsChanged,
            &dialog,
            &ExportSettingsDialog::forcedUpdate);
    }

    const auto itemData = widget ? widget->item()->data() : common::LayoutItemData();
    dialog.setMediaParams(camera, itemData);

    if (dialog.exec() != QDialog::Accepted)
        return;

    auto exportToolInstance = prepareExportTool(dialog);
    runExport(std::move(exportToolInstance));
}

void WorkbenchExportHandler::handleExportBookmarksAction()
{
    const auto parameters = menu()->currentParameters(sender());
    auto bookmarks = parameters.argument<QnCameraBookmarkList>(Qn::CameraBookmarkListRole);
    if (bookmarks.empty())
        return;

    QnTimePeriodList periods;
    for (const auto& bookmark: bookmarks)
        periods.includeTimePeriod({bookmark.startTimeMs, bookmark.durationMs});

    const auto boundingPeriod = periods.boundingPeriod();

    ExportSettingsDialog dialog(
        boundingPeriod,
        /*bookmark*/ {},
        d->fileNameValidator(),
        context()->watermark(),
        mainWindowWidget());

    for (const auto& camera: parameters.resources().filtered<QnVirtualCameraResource>())
    {
        if (const auto loader = initDataLoader(camera))
        {
            connect(
                loader.get(),
                &core::CachingCameraDataLoader::periodsChanged,
                &dialog,
                &ExportSettingsDialog::forcedUpdate);
        }
    }

    static const QString reason =
        tr("Several bookmarks can be exported as layout only.");
    dialog.disableTab(ExportMode::media, reason);
    dialog.setLayout(layoutFromBookmarks(bookmarks, resourcePool()));
    dialog.setBookmarks(bookmarks);

    if (dialog.exec() != QDialog::Accepted)
        return;

    auto exportToolInstance = prepareExportTool(dialog);
    runExport(std::move(exportToolInstance));
}

void WorkbenchExportHandler::runExport(ExportToolInstance&& context)
{
    nx::Uuid exportProcessId;
    std::unique_ptr<AbstractExportTool> exportTool;

    std::tie(exportProcessId, exportTool) = std::move(context);

    NX_ASSERT(exportTool);
    NX_ASSERT(!exportProcessId.isNull(), "Workflow is broken");
    if (exportProcessId.isNull())
        return;

    bool savingExistingLayout = d->runningExports[exportProcessId].saveExistingLayout;

    d->startingExportMessageBox.reset(new QnMessageBox(QnMessageBoxIcon::Information,
        savingExistingLayout ? tr("Starting saving...") : tr("Starting export..."),
        tr("We are preparing files for the export process. Please wait for a few seconds."),
        QDialogButtonBox::NoButton, QDialogButtonBox::NoButton, mainWindowWidget()));

    // Only show this message if startExport() takes some time.
    // startExport() will call processEvents().
    QTimer::singleShot(300, d->startingExportMessageBox.get(), &QDialog::show);

    d->exportManager->startExport(exportProcessId, std::move(exportTool));

    // If no processEvents() called, the messageBox is destroyed and no timer will be called.
    d->startingExportMessageBox.reset();

    const auto info = d->exportManager->info(exportProcessId);

    switch (info.status)
    {
    case ExportProcessStatus::initial:
    case ExportProcessStatus::exporting:
    case ExportProcessStatus::cancelling:
        if (const auto dialog = d->runningExports.value(exportProcessId).progressDialog)
            dialog->show();
        // Fill dialog with initial values (export is already running).
        exportProcessUpdated(info);
        break;
    case ExportProcessStatus::success:
    case ExportProcessStatus::failure:
        // Possibly export is finished already.
        exportProcessFinished(info);
        break;
    default:
        NX_ASSERT(false, "Should never get here in 'cancelled' state");
        break;
    }
}

WorkbenchExportHandler::ExportToolInstance WorkbenchExportHandler::prepareExportTool(
    Settings settings,
    const QnResourcePtr& resource,
    bool saveExistingLayout)
{
    const auto exportId = d->createExportContext(settings, resource, saveExistingLayout);
    std::unique_ptr<AbstractExportTool> tool;
    if (auto mediaSettings = std::get_if<ExportMediaSettings>(&settings))
    {
        tool.reset(new ExportMediaTool(*mediaSettings, resource.dynamicCast<QnMediaResource>()));
    }
    else if (auto layoutSettings = std::get_if<ExportLayoutSettings>(&settings))
    {
        tool.reset(new ExportLayoutTool(*layoutSettings, resource.dynamicCast<core::LayoutResource>()));
    }
    else
    {
        NX_ASSERT(0, "Unexpected data type for export settings");
        return {};
    }

    return std::make_pair(exportId, std::move(tool));
}

WorkbenchExportHandler::ExportToolInstance WorkbenchExportHandler::prepareExportTool(
    const ExportSettingsDialog& dialog)
{
    nx::Uuid exportId;
    std::unique_ptr<AbstractExportTool> tool;

    switch (dialog.mode())
    {
        case ExportMode::media:
        {
            auto settings = dialog.exportMediaSettings();

            if (FileExtensionUtils::isLayout(settings.fileName.extension))
            {
                ExportLayoutSettings layoutSettings;
                layoutSettings.fileName = settings.fileName;
                layoutSettings.mode = ExportLayoutSettings::Mode::Export;
                layoutSettings.period = settings.period;
                layoutSettings.readOnly = false;
                layoutSettings.watermark = settings.transcodingSettings.watermark;

                // Extract encryption options from layout-specific settings.
                layoutSettings.encryption = dialog.exportLayoutSettings().encryption;

                // Forcing camera rotation to match a rotation, used for camera in export preview.
                // This rotation properly matches either to:
                //  - export from the scene, uses rotation from the layout.
                //  - export from bookmark. Matches rotation from camera settings.
                const auto& resourcePtr = dialog.mediaResource();
                core::LayoutResourcePtr layout = layoutFromResource(resourcePtr);
                auto layoutItems = layout->getItems();
                if (!layoutItems.empty())
                {
                    common::LayoutItemData item = *layoutItems.begin();
                    item.rotation = settings.transcodingSettings.rotation;
                    layout->updateItem(item);
                }
                exportId = d->createExportContext(layoutSettings,
                    layout->toSharedPointer(),
                    /*saveExistingLayout*/ false);
                tool.reset(new ExportLayoutTool(layoutSettings, layout));
            }
            else
            {
                exportId = d->createExportContext(settings,
                    dialog.mediaResource(),
                    /*saveExistingLayout*/ false);

                tool.reset(new ExportMediaTool(settings, dialog.mediaResource()));
            }
            break;
        }
        case ExportMode::layout:
        {
            const auto settings = dialog.exportLayoutSettings();
            exportId = d->createExportContext(
                settings,
                dialog.layout()->toSharedPointer(),
                /*saveExistingLayout*/ false);
            tool.reset(new ExportLayoutTool(settings, dialog.layout()));
            break;
        }
        default:
            NX_ASSERT(false, "Unhandled export mode");
    }

    return std::make_pair(exportId, std::move(tool));
}

void WorkbenchExportHandler::at_exportStandaloneClientAction_triggered()
{
    // Lines are intentionally untranslatable for developer-only functionality.
    auto dialog = createSelfDestructingDialog<QnCustomFileDialog>(
        mainWindowWidget(),
        "Export Standalone Client",
        nx::branding::desktopClientLauncherName(),
        QnCustomFileDialog::createFilter("Executable file", "exe"));
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setAcceptMode(QFileDialog::AcceptSave);

    connect(
        dialog,
        &QnCustomFileDialog::accepted,
        this,
        [this, dialog]
        {
            constexpr auto kExeExtension = ".exe";
            constexpr auto kTmpExtension = ".tmp";

            QString targetFilename = dialog->selectedFile();
            if (!targetFilename.endsWith(kExeExtension))
                targetFilename = targetFilename + kExeExtension;

            QString temporaryFilename = targetFilename;
            temporaryFilename.replace(kExeExtension, kTmpExtension);
            #if defined(Q_OS_WIN)
                if (QnNovLauncher::createLaunchingFile(
                        temporaryFilename,
                        QnNovLauncher::ExportMode::standaloneClient) != QnNovLauncher::ErrorCode::Ok)
                {
                    QnMessageBox::critical(mainWindowWidget(),
                        QString("File %1 cannot be written").arg(temporaryFilename));
                    return;
                }
            #endif

            QFile::rename(temporaryFilename, targetFilename);
            QnMessageBox::success(mainWindowWidget(), QString("Export completed"));
        });

    dialog->show();
}

//////////////////////////////////////////////////////////////////////////
// Wonderful legacy

bool WorkbenchExportHandler::validateItemTypes(const QnLayoutResourcePtr& layout) const
{
    bool hasImage = false;
    bool hasLocal = false;

    for (const common::LayoutItemData& item: layout->getItems())
    {
        QnResourcePtr resource = core::getResourceByDescriptor(item.resource);
        if (!resource)
            continue;
        if (resource->getParentResource() == layout)
            continue;
        hasImage |= resource->hasFlags(Qn::still_image);
        hasLocal |= resource->hasFlags(Qn::local);
        if (hasImage || hasLocal)
            break;
    }

    const auto showWarning =
        [this]()
        {
            QnMessageBox::warning(mainWindowWidget(),
                tr("Local files not allowed for Multi-Video export"),
                tr("Please remove all local files from the layout and try again.")
            );
        };

    if (hasImage)
    {
        showWarning();
        return false;
    }

    if (hasLocal)
    {
        /* Always allow export selected area. */
        if (layout->getItems().size() == 1)
            return true;

        showWarning();
        return false;
    }
    return true;
}

void WorkbenchExportHandler::at_saveLocalLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto layout = parameters.resource().dynamicCast<core::LayoutResource>();

    NX_ASSERT(layout && layout->isFile());
    if (!layout || !layout->isFile())
        return;

    if (layout::isEncrypted(layout))
    {
        if (!layout::confirmPassword(layout, mainWindowWidget()))
            return; //< Reconfirm the password from user and exit if it is invalid.
    }

    const bool readOnly = !ResourceAccessManager::hasPermissions(layout, Qn::WritePermission);

    ExportLayoutSettings layoutSettings;
    layoutSettings.fileName = Filename::parse(layout->getUrl());
    layoutSettings.mode = ExportLayoutSettings::Mode::LocalSave;
    layoutSettings.period = layout->localRange();
    layoutSettings.readOnly = readOnly;
    layoutSettings.encryption = {layout::isEncrypted(layout), layout::password(layout)};

    std::unique_ptr<nx::vms::client::desktop::AbstractExportTool> exportTool;
    exportTool.reset(new ExportLayoutTool(layoutSettings, layout));

    nx::Uuid exportId = d->createExportContext(layoutSettings, layout,
        /*saveExistingLayout*/ true);

    runExport(std::make_pair(exportId, std::move(exportTool)));
}

} // namespace nx::vms::client::desktop
