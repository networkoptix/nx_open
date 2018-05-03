
#include "search_bookmarks_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>
#include <ui/style/resource_icon_cache.h>

#include <utils/camera/bookmark_helpers.h>
#include <utils/camera/camera_names_watcher.h>
#include <nx/client/core/utils/human_readable.h>
#include <nx/utils/collection.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/algorithm/index_of.h>

namespace
{

static constexpr int kInvalidSortingColumn = -1;
static constexpr int kMaxVisibleRows = 1000;

QnBookmarkSortOrder calculateSortOrder(int modelColumn, Qt::SortOrder order)
{
    const auto column =
        [modelColumn]() -> Qn::BookmarkSortField
        {
            switch(modelColumn)
            {
                case QnSearchBookmarksModel::kName:
                    return Qn::BookmarkName;
                case QnSearchBookmarksModel::kStartTime:
                    return Qn::BookmarkStartTime;
                case QnSearchBookmarksModel::kLength:
                    return Qn::BookmarkDuration;
                case QnSearchBookmarksModel::kCreationTime:
                    return Qn::BookmarkCreationTime;
                case QnSearchBookmarksModel::kCreator:
                    return Qn::BookmarkCreator;
                case QnSearchBookmarksModel::kTags:
                    return Qn::BookmarkTags;
                case QnSearchBookmarksModel::kCamera:
                    return Qn::BookmarkCameraName;
                default:
                    NX_ASSERT(false, Q_FUNC_INFO, "Wrong column");
                    return Qn::BookmarkStartTime;
            }
        }();

   return QnBookmarkSortOrder(column, order);
}

} // namespace

class QnSearchBookmarksModelPrivate:
    private QObject,
    public QnWorkbenchContextAware
{
    Q_DECLARE_PUBLIC(QnSearchBookmarksModel)
    QnSearchBookmarksModel* q_ptr = nullptr;

public:
    QnSearchBookmarksModelPrivate(QnSearchBookmarksModel* owner);

    void setRange(qint64 utcStartTimeMs, qint64 utcFinishTimeMs);

    void setFilterText(const QString& text);

    void setCameras(const QnVirtualCameraResourceList& cameras);

    const QnVirtualCameraResourceList& cameras() const;

    void applyFilter();

    void sort(int column, Qt::SortOrder order);

    int rowCount(const QModelIndex& parent) const;

    int columnCount(const QModelIndex& parent) const;

    QVariant getData(const QModelIndex& index, int role) const;

    QDateTime displayTime(qint64 millisecondsSinceEpoch) const;

    void cancelUpdatingOperation();

private:
    int getBookmarkIndex(const QnUuid& bookmarkId) const;
    QnRaiiGuardPtr startUpdateOperation();
    bool isUpdating() const;

private:
    using UniqIdToStringHash = QHash<QString, QString>;
    using UpdatingOperationWeakGuard = QWeakPointer<QnRaiiGuard>;

    UpdatingOperationWeakGuard m_updatingWeakGuard;
    QnCameraBookmarkSearchFilter m_filter;
    QnCameraBookmarksQueryPtr m_query;
    QnCameraBookmarkList m_bookmarks;
    QnVirtualCameraResourceList m_cameras;
    mutable utils::QnCameraNamesWatcher m_cameraNamesWatcher;
};

QnSearchBookmarksModelPrivate::QnSearchBookmarksModelPrivate(QnSearchBookmarksModel* owner):
    QObject(owner),
    QnWorkbenchContextAware(owner),
    q_ptr(owner),
    m_cameraNamesWatcher(commonModule())
{
    m_filter.limit = kMaxVisibleRows;
    m_filter.orderBy = QnSearchBookmarksModel::defaultSortOrder();

    connect(&m_cameraNamesWatcher, &utils::QnCameraNamesWatcher::cameraNameChanged, this,
        [this](/*const QString &cameraUuid*/)
        {
            if (m_updatingWeakGuard)
                return;

            Q_Q(QnSearchBookmarksModel);
            const auto topIndex = q->index(0);
            const auto bottomIndex = q->index(m_bookmarks.size() - 1);
            emit q->dataChanged(topIndex, bottomIndex);
        });

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved, this,
        [this](const QnUuid& bookmarkId)
        {
            if (m_updatingWeakGuard)
                return;

            const auto index = getBookmarkIndex(bookmarkId);
            if (index < 0)
                return;

            Q_Q(QnSearchBookmarksModel);
            q->beginRemoveRows(QModelIndex(), index, index);
            m_bookmarks.removeAt(index);
            q->endRemoveRows();
        });

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkUpdated, this,
        [this](const QnCameraBookmark& bookmark)
        {
            if (m_updatingWeakGuard)
                return;

            const auto index = getBookmarkIndex(bookmark.guid);
            if (index < 0)
                return;

            Q_Q(QnSearchBookmarksModel);
            q->beginInsertRows(QModelIndex(), index, index);
            m_bookmarks[index] = bookmark;
            q->endInsertRows();
        });
}

int QnSearchBookmarksModelPrivate::getBookmarkIndex(const QnUuid& bookmarkId) const
{
    return nx::utils::algorithm::index_of(m_bookmarks,
        [bookmarkId](const QnCameraBookmark& item) { return bookmarkId == item.guid; });
}

QDateTime QnSearchBookmarksModelPrivate::displayTime(qint64 millisecondsSinceEpoch) const
{
    const auto timeWatcher = context()->instance<QnWorkbenchServerTimeWatcher>();
    return timeWatcher->displayTime(millisecondsSinceEpoch);
}

void QnSearchBookmarksModelPrivate::setRange(qint64 utcStartTimeMs, qint64 utcFinishTimeMs)
{
    m_filter.startTimeMs = utcStartTimeMs;
    m_filter.endTimeMs = utcFinishTimeMs;
}

void QnSearchBookmarksModelPrivate::setFilterText(const QString& text)
{
    m_filter.text = text;
}

const QnVirtualCameraResourceList& QnSearchBookmarksModelPrivate::cameras() const
{
    return m_cameras;
}

void QnSearchBookmarksModelPrivate::setCameras(const QnVirtualCameraResourceList& cameras)
{
    m_cameras = cameras;
}

void QnSearchBookmarksModelPrivate::cancelUpdatingOperation()
{
    if (!m_updatingWeakGuard)
        return;

    m_bookmarks.clear();

    m_updatingWeakGuard.lock()->finalize();
    m_updatingWeakGuard = UpdatingOperationWeakGuard();
}

bool QnSearchBookmarksModelPrivate::isUpdating() const
{
    return m_updatingWeakGuard;
}

QnRaiiGuardPtr QnSearchBookmarksModelPrivate::startUpdateOperation()
{
    if (isUpdating())
        cancelUpdatingOperation();

    Q_Q(QnSearchBookmarksModel);

    q->beginResetModel();
    const auto guard = QnRaiiGuard::createDestructible(
        [this, q]()
        { q->endResetModel(); });

    m_updatingWeakGuard = UpdatingOperationWeakGuard(guard);
    return guard;
}

void QnSearchBookmarksModelPrivate::applyFilter()
{
    const auto endUpdateOperationGuard = startUpdateOperation();

    m_query = qnCameraBookmarksManager->createQuery();
    m_query->setFilter(m_filter);
    m_query->setCameras(m_cameras.toSet());
    m_query->executeRemoteAsync(
        [this, endUpdateOperationGuard](bool success, const QnCameraBookmarkList& bookmarks)
        {
            if (m_updatingWeakGuard.lock().data() != endUpdateOperationGuard.data())
                return; //< Operation was cancelled

            if (success)
                m_bookmarks = bookmarks;
            else
                m_bookmarks.clear();
        });
}

void QnSearchBookmarksModelPrivate::sort(int column, Qt::SortOrder order)
{
    m_filter.orderBy = calculateSortOrder(column, order);
    if (m_bookmarks.size() < kMaxVisibleRows)   // All bookmarks should be loaded here
    {
        Q_Q(QnSearchBookmarksModel);

        QnCameraBookmark::sortBookmarks(commonModule(), m_bookmarks, m_filter.orderBy);

        const auto startIndex = q->index(0, 0);
        const auto finishIndex = q->index(m_bookmarks.size() - 1,
            QnSearchBookmarksModel::kColumnsCount - 1);
        emit q->dataChanged(startIndex, finishIndex);
    }
    else
        applyFilter();
}

int QnSearchBookmarksModelPrivate::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_bookmarks.size();
}

int QnSearchBookmarksModelPrivate::columnCount(const QModelIndex& /*parent*/) const
{
    return QnSearchBookmarksModel::kColumnsCount;
}

QVariant QnSearchBookmarksModelPrivate::getData(const QModelIndex& index, int role) const
{
    static const QSet<int> kAcceptedRolesSet =
        { Qt::DecorationRole, Qt::DisplayRole, Qt::ToolTipRole, Qn::CameraBookmarkRole };

    const int row = index.row();
    if (row >= m_bookmarks.size() || !kAcceptedRolesSet.contains(role))
        return QVariant();

    const QnCameraBookmark& bookmark = m_bookmarks.at(row);

    if (role == Qn::CameraBookmarkRole)
        return QVariant::fromValue(bookmark);


    if (role == Qt::DecorationRole)
    {
        if (index.column() == QnSearchBookmarksModel::kCamera)
            return qnResIconCache->icon(QnResourceIconCache::Camera);
        if (index.column() == QnSearchBookmarksModel::kCreator)
        {
            if (bookmark.isCreatedInOlderVMS())
                return QVariant();

            return bookmark.isCreatedBySystem()
                ? qnResIconCache->icon(QnResourceIconCache::CurrentSystem)
                : qnResIconCache->icon(QnResourceIconCache::User);
        }

        return QVariant();
    }

    switch(index.column())
    {
        case QnSearchBookmarksModel::kName:
            return bookmark.name;
        case QnSearchBookmarksModel::kStartTime:
            return displayTime(bookmark.startTimeMs);
        case QnSearchBookmarksModel::kCreationTime:
            return displayTime(bookmark.creationTimeMs());
        case QnSearchBookmarksModel::kCreator:
            return helpers::getBookmarkCreatorName(bookmark, resourcePool());
        case QnSearchBookmarksModel::kLength:
        {
            const auto duration = std::chrono::milliseconds(std::abs(bookmark.durationMs));
            using HumanReadable = nx::client::core::HumanReadable;
            return HumanReadable::timeSpan(duration,
                HumanReadable::DaysAndTime,
                HumanReadable::SuffixFormat::Full,
                lit(", "),
                HumanReadable::kNoSuppressSecondUnit);
        }
        case QnSearchBookmarksModel::kTags:
            return QnCameraBookmark::tagsToString(bookmark.tags);
        case QnSearchBookmarksModel::kCamera:
            return m_cameraNamesWatcher.getCameraName(bookmark.cameraId);
        default:
            return QString();
    }
}

QnSearchBookmarksModel::QnSearchBookmarksModel(QObject* parent):
    QAbstractItemModel(parent),
    d_ptr(new QnSearchBookmarksModelPrivate(this))
{
}

QnSearchBookmarksModel::~QnSearchBookmarksModel()
{
}

QnBookmarkSortOrder QnSearchBookmarksModel::defaultSortOrder()
{
     return QnBookmarkSortOrder(Qn::BookmarkStartTime, Qt::DescendingOrder);
}

int QnSearchBookmarksModel::sortFieldToColumn(Qn::BookmarkSortField field)
{
    switch (field)
    {
        case Qn::BookmarkName:
            return Column::kName;
        case Qn::BookmarkStartTime:
            return Column::kStartTime;
        case Qn::BookmarkDuration:
            return Column::kLength;
        case Qn::BookmarkCreationTime:
            return Column::kCreationTime;
        case Qn::BookmarkCreator:
            return Column::kCreator;
        case Qn::BookmarkTags:
            return Column::kTags;
        case Qn::BookmarkCameraName:
            return Column::kCamera;
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Unhandled field");
            break;
    }
    return kInvalidSortingColumn;
}

void QnSearchBookmarksModel::applyFilter()
{
    Q_D(QnSearchBookmarksModel);
    d->applyFilter();
}

void QnSearchBookmarksModel::setRange(qint64 utcStartTimeMs, qint64 utcFinishTimeMs)
{
    Q_D(QnSearchBookmarksModel);
    d->setRange(utcStartTimeMs, utcFinishTimeMs);
}

void QnSearchBookmarksModel::setFilterText(const QString& text)
{
    Q_D(QnSearchBookmarksModel);
    d->setFilterText(text);
}

void QnSearchBookmarksModel::setCameras(const QnVirtualCameraResourceList& cameras)
{
    Q_D(QnSearchBookmarksModel);
    d->setCameras(cameras);
}

const QnVirtualCameraResourceList& QnSearchBookmarksModel::cameras() const
{
    Q_D(const QnSearchBookmarksModel);
    return d->cameras();
}

void QnSearchBookmarksModel::cancelUpdateOperation()
{
    Q_D(QnSearchBookmarksModel);
    d->cancelUpdatingOperation();
}

int QnSearchBookmarksModel::rowCount(const QModelIndex& parent) const
{
    Q_D(const QnSearchBookmarksModel);
    return d->rowCount(parent);
}

int QnSearchBookmarksModel::columnCount(const QModelIndex& parent) const
{
    Q_D(const QnSearchBookmarksModel);
    return d->columnCount(parent);
}

void QnSearchBookmarksModel::sort(int column, Qt::SortOrder order)
{
    Q_D(QnSearchBookmarksModel);
    d->sort(column, order);
}

QModelIndex QnSearchBookmarksModel::index(int row, int column, const QModelIndex& parent) const
{
    return hasIndex(row, column, parent)
        ? createIndex(row, column, nullptr)
        : QModelIndex();
}

QModelIndex QnSearchBookmarksModel::parent(const QModelIndex& /*child*/) const
{
    return QModelIndex();
}

QVariant QnSearchBookmarksModel::data(const QModelIndex& index, int role) const
{
    Q_D(const QnSearchBookmarksModel);
    return d->getData(index, role);
}

QVariant QnSearchBookmarksModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section >= kColumnsCount)
        return QAbstractItemModel::headerData(section, orientation, role);

    switch (section)
    {
        case kName:
            return tr("Name");
        case kCamera:
            return tr("Camera");
        case kStartTime:
            return tr("Start time");
        case kLength:
            return tr("Length");
        case kCreationTime:
            return tr("Created");
        case kCreator:
            return tr("Creator");
        case kTags:
            return tr("Tags");
        default:
            NX_ASSERT(false, "Invalid column identifier");
            return QString();
    }
}
