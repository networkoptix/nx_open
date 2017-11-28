#include "legacy_workbench_export_handler.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>

#include <client/client_settings.h>
#include <client/client_module.h>

#include <common/common_globals.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <camera/camera_data_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/fusion/model_functions.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <platform/environment.h>

#include <recording/time_period.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

#include <nx/client/desktop/legacy/legacy_export_layout_tool.h>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/event_processors.h>

#include <nx/utils/app_info.h>
#include <nx/utils/string.h>

#include <utils/common/app_info.h>

namespace {

static const QString filterSeparator(QLatin1String(";;"));

/** Default bitrate for exported file size estimate in megabytes per second. */
static const qreal kDefaultBitrateMBps = 0.5;

/** Exe files greater than 4 Gb are not working. */
static const qint64 kMaximimumExeFileSizeMb = 4096;

/** Reserving 200 Mb for client binaries. */
static const qint64 kReservedClientSizeMb = 200;

static const int kMsPerSecond = 1000;

static const int kBitsPerByte = 8;


/** Calculate estimated video size in megabytes. */
qint64 estimatedExportVideoSizeMb(const QnMediaResourcePtr& mediaResource, qint64 lengthMs)
{
    qreal maxBitrateMBps = kDefaultBitrateMBps;

    if (QnVirtualCameraResourcePtr camera = mediaResource.dynamicCast<QnVirtualCameraResource>())
    {
        auto bitrateInfos = QJson::deserialized<CameraBitrates>(
            camera->getProperty(Qn::CAMERA_BITRATE_INFO_LIST_PARAM_NAME).toUtf8());
        if (!bitrateInfos.streams.empty())
            maxBitrateMBps = std::max_element(
                bitrateInfos.streams.cbegin(), bitrateInfos.streams.cend(),
                [](const CameraBitrateInfo& l, const CameraBitrateInfo &r)
        {
            return l.actualBitrate < r.actualBitrate;
        })->actualBitrate;
    }

    return maxBitrateMBps * lengthMs / (kMsPerSecond * kBitsPerByte);
}

}

namespace nx {
namespace client {
namespace desktop {
namespace legacy {

// -------------------------------------------------------------------------- //
// WorkbenchExportHandler
// -------------------------------------------------------------------------- //
WorkbenchExportHandler::WorkbenchExportHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(ui::action::SaveLocalLayoutAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_saveLocalLayoutAction_triggered);
    connect(action(ui::action::SaveLocalLayoutAsAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_saveLocalLayoutAsAction_triggered);
}

QString WorkbenchExportHandler::binaryFilterName() const
{
#ifdef Q_OS_WIN
#ifdef Q_OS_WIN64
    return tr("Executable %1 Media File (x64) (*.exe)").arg(QnAppInfo::organizationName());
#else
    return tr("Executable %1 Media File (x86) (*.exe)").arg(QnAppInfo::organizationName());
#endif
#endif
    return QString();
}

bool WorkbenchExportHandler::lockFile(const QString &filename)
{
    if (m_filesIsUse.contains(filename))
    {
        QnMessageBox::warning(mainWindow(),
            tr("File already used for recording"),
            QFileInfo(filename).completeBaseName()
                + L'\n' + tr("Please choose another name or wait until recording is finished."));
        return false;
    }

    if (QFile::exists(filename) && !QFile::remove(filename))
    {

        QnFileMessages::overwriteFailed(mainWindow(),
            QFileInfo(filename).completeBaseName());
        return false;
    }

    m_filesIsUse << filename;
    return true;
}

void WorkbenchExportHandler::unlockFile(const QString &filename)
{
    m_filesIsUse.remove(filename);
}

bool WorkbenchExportHandler::isBinaryExportSupported() const
{
    if (nx::utils::AppInfo::isWindows())
        return !qnRuntime->isActiveXMode();
    return false;
}

bool WorkbenchExportHandler::saveLayoutToLocalFile(const QnLayoutResourcePtr &layout,
    const QnTimePeriod &exportPeriod,
    const QString &layoutFileName,
    ExportLayoutSettings::Mode mode,
    bool readOnly,
    bool cancellable)
{
    if (!validateItemTypes(layout))
        return false;

    QnProgressDialog* exportProgressDialog = new QnProgressDialog(mainWindow(), Qt::Dialog);

    if (!cancellable)
    {
        exportProgressDialog->setCancelButton(NULL);

        QnMultiEventEater *eventEater = new QnMultiEventEater(Qn::IgnoreEvent, exportProgressDialog);
        eventEater->addEventType(QEvent::KeyPress);
        eventEater->addEventType(QEvent::KeyRelease); /* So that ESC doesn't close the dialog. */
        eventEater->addEventType(QEvent::Close);
        exportProgressDialog->installEventFilter(eventEater);
    }

    exportProgressDialog->setWindowTitle(tr("Exporting Layout"));
    exportProgressDialog->setModal(false);
    exportProgressDialog->show();

    const auto filename = Filename::parse(layoutFileName);
    auto tool = new ExportLayoutTool({layout, exportPeriod, filename, mode, readOnly}, this);
    connect(tool, &ExportLayoutTool::rangeChanged, exportProgressDialog,
        &QnProgressDialog::setRange);
    connect(tool, &ExportLayoutTool::valueChanged, exportProgressDialog,
        &QnProgressDialog::setValue);
    connect(tool, &ExportLayoutTool::stageChanged, exportProgressDialog,
        &QnProgressDialog::setLabelText);

    connect(tool, &ExportLayoutTool::finished, exportProgressDialog, &QWidget::hide);
    connect(tool, &ExportLayoutTool::finished, exportProgressDialog, &QObject::deleteLater);
    connect(tool, &ExportLayoutTool::finished, this, &WorkbenchExportHandler::at_layout_exportFinished);
    connect(tool, &ExportLayoutTool::finished, tool, &QObject::deleteLater);
    connect(exportProgressDialog, &QnProgressDialog::canceled, tool, &ExportLayoutTool::stop);

    return tool->start();
}

bool WorkbenchExportHandler::exeFileIsTooBig(
    const QnMediaResourcePtr& mediaResource,
    const QnTimePeriod& period) const
{
    qint64 videoSizeMb = estimatedExportVideoSizeMb(mediaResource, period.durationMs);
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

bool WorkbenchExportHandler::exeFileIsTooBig(
    const QnLayoutResourcePtr& layout,
    const QnTimePeriod& period) const
{
    qint64 videoSizeMb = 0;
    for (const auto& resource: layout->layoutResources())
    {
        if (const QnMediaResourcePtr& media = resource.dynamicCast<QnMediaResource>())
            videoSizeMb += estimatedExportVideoSizeMb(media, period.durationMs);
    }
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

bool WorkbenchExportHandler::confirmExportTooBigExeFile() const
{
    return confirmExport(QnMessageBoxIcon::Warning,
        tr("EXE format not recommended"),
        tr("EXE files over 4 GB cannot be opened by double click due to a Windows limitation.")
            + L'\n' + tr("Export to EXE anyway?"));
}

void WorkbenchExportHandler::at_layout_exportFinished(bool success, const QString &filename)
{
    unlockFile(filename);

    auto tool = dynamic_cast<ExportLayoutTool*>(sender());
    if (!tool)
        return;

    if (!success && !tool->errorMessage().isEmpty())
    {
        QnMessageBox::critical(mainWindow(),
            tr("Failed to export Multi-Video"), tool->errorMessage());
    }
}

bool WorkbenchExportHandler::validateItemTypes(const QnLayoutResourcePtr &layout)
{
    bool hasImage = false;
    bool hasLocal = false;

    for (const QnLayoutItemData &item : layout->getItems())
    {
        QnResourcePtr resource = resourcePool()->getResourceByDescriptor(item.resource);
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
            QnMessageBox::warning(mainWindow(),
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

bool WorkbenchExportHandler::saveLocalLayout(const QnLayoutResourcePtr &layout, bool readOnly, bool cancellable)
{
    return saveLayoutToLocalFile(layout,
        layout->getLocalRange(),
        layout->getUrl(),
        ExportLayoutSettings::Mode::LocalSave,
        readOnly,
        cancellable);
}

bool WorkbenchExportHandler::doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod,
    const QnLayoutResourcePtr &layout,
    ExportLayoutSettings::Mode mode)
{
    // TODO: #Elric we have a lot of copypasta with at_exportTimeSelectionAction_triggered

    bool wasLoggedIn = !context()->user().isNull();

    if (!validateItemTypes(layout))
        return false;

    NX_EXPECT(mode == ExportLayoutSettings::Mode::LocalSaveAs);
    QString dialogName = tr("Save local layout as...");

    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();

    QString baseName = layout->getName();
    if (qnRuntime->isActiveXMode() || baseName.isEmpty())
        baseName = tr("exported");
    QString suggestion = nx::utils::replaceNonFileNameCharacters(baseName, QLatin1Char('_'));
    suggestion = QnEnvironment::getUniqueFileName(previousDir, QFileInfo(suggestion).baseName());

    QString fileName;
    bool readOnly = false;

    QString mediaFileFilter = tr("%1 Media File (*.nov)").arg(QnAppInfo::organizationName());
    if (isBinaryExportSupported())
        mediaFileFilter += filterSeparator + binaryFilterName();

    while (true)
    {
        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        QScopedPointer<QnSessionAwareFileDialog> dialog(new QnSessionAwareFileDialog(
            mainWindow(),
            dialogName,
            suggestion,
            mediaFileFilter
            ));
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        dialog->addCheckBox(tr("Make file read-only."), &readOnly);

        setHelpTopic(dialog.data(), Qn::Exporting_Layout_Help);

        if (!dialog->exec())
            return false;

        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return false;

        QString selectedExtension = dialog->selectedExtension();
        bool binaryExport = isBinaryExportSupported()
            ? selectedExtension.contains(lit(".exe"))
            : false;

        if (binaryExport
            && exeFileIsTooBig(layout, exportPeriod)
            && !confirmExportTooBigExeFile())
                continue;

        if (!fileName.toLower().endsWith(selectedExtension))
        {
            fileName += selectedExtension;

            // method called under condition because in other case this message is popped out by the dialog itself
            if (QFile::exists(fileName) &&
                !QnFileMessages::confirmOverwrite(
                    mainWindow(), QFileInfo(fileName).completeBaseName()))
            {
                return false;
            }
        }

        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        if (!lockFile(fileName))
            continue;

        break;
    }

    /* Check if we were disconnected (server shut down) while the dialog was open.
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return false;

    qnSettings->setLastExportDir(QFileInfo(fileName).absolutePath());

    saveLayoutToLocalFile(layout, exportPeriod, fileName, mode, readOnly, true);

    return true;
}

bool WorkbenchExportHandler::confirmExport(
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras) const
{
    QnMessageBox dialog(icon, text, extras,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindow());

    dialog.addButton(tr("Export"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    return (dialog.exec() != QDialogButtonBox::Cancel);
}

void WorkbenchExportHandler::at_saveLocalLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto layout = parameters.resource().dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout && layout->isFile());
    if (!layout || !layout->isFile())
        return;

    const bool readOnly = !accessController()->hasPermissions(layout,
        Qn::WritePermission);
    saveLocalLayout(layout, readOnly, true); //< Overwrite layout file.
}

void WorkbenchExportHandler::at_saveLocalLayoutAsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto layout = parameters.resource().dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout && layout->isFile());
    if (!layout || !layout->isFile())
        return;

    doAskNameAndExportLocalLayout(layout->getLocalRange(),
        layout,
        ExportLayoutSettings::Mode::LocalSaveAs);
}

} // namespace legacy
} // namespace desktop
} // namespace client
} // namespace nx
