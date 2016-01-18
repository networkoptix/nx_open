
#include "search_bookmarks_dialog.h"

#include "ui_search_bookmarks_dialog.h"

#include <recording/time_period.h>

#include <common/common_module.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/workbench/watchers/current_user_available_cameras_watcher.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/common/read_only.h>
#include <ui/models/search_bookmarks_model.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <client/client_settings.h>
#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>

namespace
{
    enum
    {
        kMillisecondsInSeconds = 1000
        , kStartDayOffsetMs = 0
        , kMillisecondsInDay = 60 * 60 * 24 * kMillisecondsInSeconds
    };

    qint64 getStartOfTheDayMs(qint64 timeMs)
    {
        return timeMs - (timeMs % kMillisecondsInDay);
    }

    qint64 getEndOfTheDayMs(qint64 timeMs)
    {
        return getStartOfTheDayMs(timeMs) + kMillisecondsInDay;
    }

    qint64 getTimeOnServerMs(QnWorkbenchContext *context
        , const QDate &clientDate
        , qint64 dayEndOffset = kStartDayOffsetMs)
    {
        if (qnSettings->timeMode() == Qn::ClientTimeMode)
        {
            // QDateTime is created from date, thus it always started
            // from the start of the day in current timezone
            return QDateTime(clientDate).toMSecsSinceEpoch() + dayEndOffset;
        }

        const auto timeWatcher = context->instance<QnWorkbenchServerTimeWatcher>();
        const auto server = qnCommon->currentServer();
        const auto serverUtcOffsetSecs = timeWatcher->utcOffset(server) / kMillisecondsInSeconds;
        const QDateTime serverTime(clientDate, QTime(0, 0), Qt::OffsetFromUTC, serverUtcOffsetSecs);
        return serverTime.toMSecsSinceEpoch() + dayEndOffset;
    }

    QDate extractDisplayDate(QnWorkbenchContext *context
        , qint64 timestamp)
    {
        return context->instance<QnWorkbenchServerTimeWatcher>()->displayTime(timestamp).date();
    };
}

class QnSearchBookmarksDialog::Impl : public QObject
    , public QnWorkbenchContextAware
{
    Q_DECLARE_TR_FUNCTIONS(Impl)

public:
    Impl(const QString &filterText
        , qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs
        , QDialog *owner);

    ~Impl();

    void updateHeadersWidth();

    void refresh();

    void setParameters(const QString &filterText
        , qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs);

private:
    QnVirtualCameraResourceList availableCameras() const;

    QnVirtualCameraResourcePtr availableCameraByUniqueId(const QString &uniqueId) const;

    void resetToAllAvailableCameras();

    void setCameras(const QnVirtualCameraResourceList &cameras);

    void chooseCamera();

    void customContextMenuRequested();

    QMenu *createContextMenu(const QnActionParameters &params);

private:
    void openInNewLayoutHandler();

    void exportBookmarkHandler();

    bool fillActionParameters(QnActionParameters &params, QnTimePeriod &window);

    bool currentUserHasAdminPrivileges();

    void applyModelChanges();

private:
    typedef QScopedPointer<Ui::BookmarksLog> UiImpl;

    QDialog * const m_owner;
    const UiImpl m_ui;
    QnSearchBookmarksModel * const m_model;

    bool m_allCamerasChoosen;

    QAction * const m_openInNewTabAction;
    QAction * const m_exportBookmarkAction;
    bool m_updatingNow;
};

///

QnSearchBookmarksDialog::Impl::Impl(const QString &filterText
    , qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs
    , QDialog *owner)

    : QObject(owner)
    , QnWorkbenchContextAware(owner)
    , m_owner(owner)
    , m_ui(new Ui::BookmarksLog())
    , m_model(new QnSearchBookmarksModel(this))

    , m_allCamerasChoosen(true)

    , m_openInNewTabAction(new QAction(tr("Open in New Tab"), this))
    , m_exportBookmarkAction(new QAction(tr("Export bookmark..."), this))
    , m_updatingNow(false)
{
    m_ui->setupUi(m_owner);
    m_ui->gridBookmarks->setModel(m_model);

    const auto updateFilterText = [this]()
    {
        m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text());
        applyModelChanges();
    };

    enum { kUpdateFilterDelayMs = 200 };
    m_ui->filterLineEdit->setTextChangedSignalFilterMs(kUpdateFilterDelayMs);

    connect(m_ui->filterLineEdit, &QnSearchLineEdit::enterKeyPressed, this, updateFilterText);
    connect(m_ui->filterLineEdit, &QnSearchLineEdit::textChanged, this, updateFilterText);

    connect(m_ui->filterLineEdit, &QnSearchLineEdit::escKeyPressed, this, [this]()
    {
        m_ui->filterLineEdit->lineEdit()->setText(QString());
        m_model->setFilterText(QString());
        applyModelChanges();
    });

    connect(m_ui->dateEditFrom, &QDateEdit::userDateChanged, this, [this](const QDate &date)
    {
        m_model->setRange(getTimeOnServerMs(context(), date)
            , getTimeOnServerMs(context(), m_ui->dateEditTo->date(), kMillisecondsInDay));
        applyModelChanges();
    });
    connect(m_ui->dateEditTo, &QDateEdit::userDateChanged, this, [this](const QDate &date)
    {
        m_model->setRange(getTimeOnServerMs(context(), m_ui->dateEditFrom->date())
            , getTimeOnServerMs(context(), date, kMillisecondsInDay));
        applyModelChanges();
    });

    connect(m_ui->refreshButton, &QPushButton::clicked, this, &Impl::refresh);
    connect(m_ui->cameraButton, &QPushButton::clicked, this, &Impl::chooseCamera);

    connect(m_ui->gridBookmarks, &QTableView::customContextMenuRequested
        , this, &Impl::customContextMenuRequested);

    connect(m_ui->gridBookmarks, &QTableView::doubleClicked, this
        , [this](const QModelIndex & /* index */)
    {
        openInNewLayoutHandler();
    });

    connect(m_openInNewTabAction, &QAction::triggered, this, &Impl::openInNewLayoutHandler);
    connect(m_exportBookmarkAction, &QAction::triggered, this, &Impl::exportBookmarkHandler);

    setParameters(filterText, getStartOfTheDayMs(utcStartTimeMs), getEndOfTheDayMs(utcFinishTimeMs));

    const auto camerasWatcher = context()->instance<QnCurrentUserAvailableCamerasWatcher>();
    connect(camerasWatcher, &QnCurrentUserAvailableCamerasWatcher::userChanged, this, [this]()
    {
        resetToAllAvailableCameras();
        applyModelChanges();
    });

    connect(camerasWatcher, &QnCurrentUserAvailableCamerasWatcher::availableCamerasChanged, this, [this]()
    {
        if (m_allCamerasChoosen)
            resetToAllAvailableCameras();
    });

    m_ui->filterLineEdit->lineEdit()->setPlaceholderText(tr("Search bookmarks by name, tag or description"));
}

QnSearchBookmarksDialog::Impl::~Impl()
{
}

void QnSearchBookmarksDialog::Impl::applyModelChanges()
{
    if (!m_updatingNow)
        m_model->applyFilter();
}

void QnSearchBookmarksDialog::Impl::setParameters(const QString &filterText
    , qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs)
{
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updatingNow, true);

        resetToAllAvailableCameras();

        m_ui->filterLineEdit->lineEdit()->setText(filterText);
        m_ui->dateEditFrom->setDate(extractDisplayDate(context(), utcStartTimeMs));
        m_ui->dateEditTo->setDate(extractDisplayDate(context(), utcFinishTimeMs));

        m_model->setFilterText(filterText);
        m_model->setRange(utcStartTimeMs, utcFinishTimeMs);
    }

    applyModelChanges();
}

bool QnSearchBookmarksDialog::Impl::fillActionParameters(QnActionParameters &params, QnTimePeriod &window)
{
    const auto index = m_ui->gridBookmarks->currentIndex();
    if (!index.isValid())
        return false;

    const auto bookmarkVariant = m_model->data(index, Qn::CameraBookmarkRole);
    if (!bookmarkVariant.isValid())
        return false;

    const auto &bookmark = bookmarkVariant.value<QnCameraBookmark>();
    const QnResourcePtr resource = availableCameraByUniqueId(bookmark.cameraId);

    if (!resource)
        return false;

    window = QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs);

    params = QnActionParameters(resource);
    params.setArgument(Qn::TimePeriodRole, window);
    params.setArgument(Qn::ItemTimeRole, window.startTimeMs);

    return true;
}

void QnSearchBookmarksDialog::Impl::openInNewLayoutHandler()
{
    QnActionParameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    const auto setFirstLayoutItemPeriod = [this](const QnTimePeriod &window
        , Qn::ItemDataRole role)
    {
        const auto layout = workbench()->currentLayout();
        const auto items = layout->items();
        if (items.empty())
            return;

        const auto item = *items.begin();
        item->setData(role, window);
    };

    menu()->trigger(Qn::OpenInNewLayoutAction, params);

    static const qint64 kMinOffset = 30 * 1000;
    const auto offset = std::max(window.durationMs, kMinOffset);

    const QnTimePeriod extendedWindow(window.startTimeMs - offset
        , window.durationMs + offset * 2);
    setFirstLayoutItemPeriod(extendedWindow, Qn::ItemSliderWindowRole);

    menu()->trigger(Qn::BookmarksModeAction);
    m_owner->close();
}

void QnSearchBookmarksDialog::Impl::exportBookmarkHandler()
{
    QnActionParameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    if (menu()->canTrigger(Qn::ExportTimeSelectionAction, params))
        menu()->trigger(Qn::ExportTimeSelectionAction, params);
}

void QnSearchBookmarksDialog::Impl::updateHeadersWidth()
{
    enum
    {
        kNamePart = 8
        , kStartTimePart = 3
        , kLengthPart = 3
        , kTagsPart = 4
        , kCameraPart = 3
        , kPartsTotalNumber = kNamePart + kStartTimePart + kLengthPart + kTagsPart + kCameraPart
    };

    QHeaderView * const header = m_ui->gridBookmarks->horizontalHeader();
    const int totalWidth = header->width();
    header->resizeSection(QnSearchBookmarksModel::kName, totalWidth * kNamePart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kStartTime, totalWidth * kStartTimePart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kLength, totalWidth * kLengthPart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kTags, totalWidth * kTagsPart / kPartsTotalNumber);
    header->resizeSection(QnSearchBookmarksModel::kCamera, totalWidth * kCameraPart / kPartsTotalNumber);
}

void QnSearchBookmarksDialog::Impl::refresh()
{
    m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text());
    m_model->setRange(getTimeOnServerMs(context(), m_ui->dateEditFrom->date())
        , getTimeOnServerMs(context(), m_ui->dateEditTo->date(), kMillisecondsInDay));

    m_model->applyFilter();
}


QnVirtualCameraResourceList QnSearchBookmarksDialog::Impl::availableCameras() const
{
    const auto camerasWatcher = context()->instance<QnCurrentUserAvailableCamerasWatcher>();
    return camerasWatcher->availableCameras();
}

QnVirtualCameraResourcePtr QnSearchBookmarksDialog::Impl::availableCameraByUniqueId(const QString &uniqueId) const
{
    const auto cameras = availableCameras();

    const auto it = std::find_if(cameras.begin(), cameras.end()
        , [this, uniqueId](const QnVirtualCameraResourcePtr &camera)
    {
        return (camera->getUniqueId() == uniqueId);
    });

    return (it == cameras.end() ? QnVirtualCameraResourcePtr() : *it);
}

void QnSearchBookmarksDialog::Impl::resetToAllAvailableCameras()
{
    setCameras(QnVirtualCameraResourceList());
}

void QnSearchBookmarksDialog::Impl::setCameras(const QnVirtualCameraResourceList &cameras)
{
    m_allCamerasChoosen = cameras.empty();
    const auto &correctCameras = (m_allCamerasChoosen
        ? availableCameras() : cameras);

    m_model->setCameras(correctCameras);

    const bool isAdmin = currentUserHasAdminPrivileges();
    setReadOnly(m_ui->cameraButton, !isAdmin);

    if (cameras.empty())
    {
        static const auto kAnyDevicesStringsSet = QnCameraDeviceStringSet(
            tr("<Any Device>"), tr("<Any Camera>"), tr("<Any IO Module>"));
        static const auto kAnyMyDevicesStringsSet = QnCameraDeviceStringSet(
            tr("<All My Devices>"), tr("<All My Cameras>"), tr("<All My IO Modules>"));

        const auto &devicesStringsSet = (isAdmin
            ? kAnyDevicesStringsSet : kAnyMyDevicesStringsSet);

        m_ui->cameraButton->setText(QnDeviceDependentStrings::getNameFromSet(
            devicesStringsSet, correctCameras));
        return;
    }

    const auto devicesStringSet = QnCameraDeviceStringSet(
        tr("<%n device(s)>", nullptr, cameras.size())
        , tr("<%n camera(s)>", nullptr, cameras.size())
        , tr("<%n IO module(s)>", nullptr, cameras.size()));

    const auto caption = QnDeviceDependentStrings::getNameFromSet(
        devicesStringSet, cameras);

    m_ui->cameraButton->setText(caption);
}

void QnSearchBookmarksDialog::Impl::chooseCamera()
{
    /// Do not allow user without administrator privileges to choose cameras
    if (!currentUserHasAdminPrivileges())
        return;

    QnResourceSelectionDialog dialog(m_owner);
    dialog.setSelectedResources(m_allCamerasChoosen
        ? QnVirtualCameraResourceList() : m_model->cameras());

    if (dialog.exec() == QDialog::Accepted)
    {
        setCameras(dialog.selectedResources().filtered<QnVirtualCameraResource>());
        m_model->applyFilter();
    }
}

bool QnSearchBookmarksDialog::Impl::currentUserHasAdminPrivileges()
{
    return accessController()->hasGlobalPermissions(Qn::GlobalAdminPermissions);
}

QMenu *QnSearchBookmarksDialog::Impl::createContextMenu(const QnActionParameters &params)
{
    auto result = new QMenu();

    const auto addActionToMenu = [this, result, params] (Qn::ActionId id, QAction *action)
    {
        if (menu()->canTrigger(id, params))
            result->addAction(action);
    };

    addActionToMenu(Qn::OpenInNewLayoutAction, m_openInNewTabAction);
    addActionToMenu(Qn::ExportTimeSelectionAction, m_exportBookmarkAction);
    return result;
}

void QnSearchBookmarksDialog::Impl::customContextMenuRequested()
{
    QnActionParameters params;
    QnTimePeriod window;
    if (!fillActionParameters(params, window))
        return;

    if (auto newMenu = createContextMenu(params))
    {
        newMenu->exec(QCursor::pos());
        newMenu->deleteLater();
    }
}

///

QnSearchBookmarksDialog::QnSearchBookmarksDialog(const QString &filterText
    , qint64 utcStartTimGeMs
    , qint64 utcFinishTimeMs
    , QWidget *parent)
    : QnWorkbenchStateDependentButtonBoxDialog(parent)
    , m_impl(new Impl(filterText, utcStartTimGeMs, utcFinishTimeMs, this))
{
}

QnSearchBookmarksDialog::~QnSearchBookmarksDialog()
{
}

void QnSearchBookmarksDialog::setParameters(qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs
    , const QString &filterText)
{
    m_impl->setParameters(filterText, getStartOfTheDayMs(utcStartTimeMs)
        , getEndOfTheDayMs(utcFinishTimeMs));
}


void QnSearchBookmarksDialog::resizeEvent(QResizeEvent *event)
{
    Base::resizeEvent(event);
    m_impl->updateHeadersWidth();
}

void QnSearchBookmarksDialog::showEvent(QShowEvent *event)
{
    Base::showEvent(event);
    m_impl->refresh();
}
