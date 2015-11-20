
#include "search_bookmarks_dialog.h"

#include "ui_search_bookmarks_dialog.h"

#include <recording/time_period.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/models/search_bookmarks_model.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

class QnSearchBookmarksDialog::Impl : public QObject
    , public QnWorkbenchContextAware
{
    Q_DECLARE_TR_FUNCTIONS(Impl)

public:
    Impl(QDialog *owner);

    ~Impl();

    void updateHeadersWidth();

    void refresh();

private:
    void chooseCamera();

    void customContextMenuRequested();

    QMenu *createContextMenu(const QnActionParameters &params
        , const QnTimePeriod &window);

private:
    void openInNewLayoutHandler();

    void exportBookmarkHandler();

    bool fillActionParameters(QnActionParameters &params, QnTimePeriod &window);

private:
    typedef QScopedPointer<Ui::BookmarksLog> UiImpl;
    
    QDialog * const m_owner;
    const UiImpl m_ui;
    QnSearchBookmarksModel * const m_model;

    QnVirtualCameraResourceList m_cameras;

    QAction * const m_openInNewTabAction;
    QAction * const m_exportBookmarkAction;
};

///

QnSearchBookmarksDialog::Impl::Impl(QDialog *owner)
    : QObject(owner)
    , QnWorkbenchContextAware(owner)
    , m_owner(owner)
    , m_ui(new Ui::BookmarksLog())
    , m_model(new QnSearchBookmarksModel(this))

    , m_cameras()

    , m_openInNewTabAction(new QAction(tr("Open in New Tab"), this))
    , m_exportBookmarkAction(new QAction(tr("Export bookmark..."), this))
{
    m_ui->setupUi(m_owner);

    const QDate now = QDateTime::currentDateTime().date();
    m_ui->dateEditFrom->setDate(now);
    m_ui->dateEditTo->setDate(now);
    m_ui->gridBookmarks->setModel(m_model);

    m_model->setDates(m_ui->dateEditFrom->date(), m_ui->dateEditTo->date());
    connect(m_ui->filterLineEdit, &QnSearchLineEdit::enterKeyPressed, this
        , [this]() 
    { 
        m_model->setFilterText(m_ui->filterLineEdit->lineEdit()->text()); 
        m_model->applyFilter();
    });
    connect(m_ui->filterLineEdit, &QnSearchLineEdit::escKeyPressed, this, [this]() 
    {
        m_ui->filterLineEdit->lineEdit()->setText(QString());
        m_model->setFilterText(QString());
        m_model->applyFilter();
    });

    connect(m_ui->dateEditFrom, &QDateEdit::userDateChanged, this, [this](const QDate &date) 
    { 
        m_model->setDates(date, m_ui->dateEditTo->date()); 
        m_model->applyFilter();
    });
    connect(m_ui->dateEditTo, &QDateEdit::userDateChanged, this, [this](const QDate &date) 
    { 
        m_model->setDates(m_ui->dateEditFrom->date(), date); 
        m_model->applyFilter();
    });
    
    connect(m_ui->refreshButton, &QPushButton::clicked, this, &Impl::refresh);
    connect(m_ui->cameraButton, &QPushButton::clicked, this, &Impl::chooseCamera);

    m_model->applyFilter();

    connect(m_ui->gridBookmarks, &QTableView::customContextMenuRequested
        , this, &Impl::customContextMenuRequested);

    connect(m_ui->gridBookmarks, &QTableView::doubleClicked, this
        , [this](const QModelIndex &index)
    {
        openInNewLayoutHandler();
    });

    connect(m_openInNewTabAction, &QAction::triggered, this, &Impl::openInNewLayoutHandler);
    connect(m_exportBookmarkAction, &QAction::triggered, this, &Impl::exportBookmarkHandler);
}

QnSearchBookmarksDialog::Impl::~Impl()
{
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
    const QnResourcePtr resource = qnResPool->getResourceByUniqueId(bookmark.cameraId);

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
    m_model->applyFilter();
}

void QnSearchBookmarksDialog::Impl::chooseCamera()
{
    QnResourceSelectionDialog dialog(m_owner);
    dialog.setSelectedResources(m_cameras);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_cameras = dialog.selectedResources().filtered<QnVirtualCameraResource>();
        m_model->setCameras(m_cameras);
        m_model->applyFilter();

        static const QString kEmptyCamerasCaption = tr("<Any camera>");
        static const QString kCamerasTemplate = tr("camera(s)");
        static const QString kCamerasCaptionTemplate = lit("<%1 %2>");
        m_ui->cameraButton->setText(m_cameras.empty() ? kEmptyCamerasCaption : 
            kCamerasCaptionTemplate.arg(QString::number(m_cameras.size()), kCamerasTemplate));
    }
}

QMenu *QnSearchBookmarksDialog::Impl::createContextMenu(const QnActionParameters &params
    , const QnTimePeriod &window)
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

    if (auto newMenu = createContextMenu(params, window))
    {
        newMenu->exec(QCursor::pos());
        newMenu->deleteLater();
    }
}

///

QnSearchBookmarksDialog::QnSearchBookmarksDialog(QWidget *parent)
    : QnButtonBoxDialog(parent)
    , m_impl(new Impl(this))
{
}

QnSearchBookmarksDialog::~QnSearchBookmarksDialog()
{
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
