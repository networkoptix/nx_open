
#include "search_bookmarks_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <utils/camera/camera_names_watcher.h>
#include <utils/common/qtimespan.h>
#include <utils/common/collection.h>

namespace
{
    typedef std::function<void ()> ResetOperationFunction;

    enum { kInvalidSortingColumn = -1 };
    enum { kMaxVisibleRows = 1000 };

    QnBookmarkSortOrder calculateSortOrder(int col
        , Qt::SortOrder order)
    {
        const auto column = [col]() -> Qn::BookmarkSortField
        {
            switch(col)
            {
            case QnSearchBookmarksModel::kName:
                return Qn::BookmarkName;
            case QnSearchBookmarksModel::kStartTime:
                return Qn::BookmarkStartTime;
            case QnSearchBookmarksModel::kLength:
                return Qn::BookmarkDuration;
            case QnSearchBookmarksModel::kTags:
                return Qn::BookmarkTags;
            case QnSearchBookmarksModel::kCamera:
                return Qn::BookmarkCameraName;
            default:
                Q_ASSERT_X(false, Q_FUNC_INFO, "Wrong column");
                return Qn::BookmarkStartTime;
            }
        }();

       return QnBookmarkSortOrder(column, order);
    }
}

class QnSearchBookmarksModel::Impl : private QObject
    , public QnWorkbenchContextAware
{
public:
    Impl(QnSearchBookmarksModel *owner
        , const ResetOperationFunction &beginResetModel
        , const ResetOperationFunction &endResetModel);

    ~Impl();

    ///
    void setRange(qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs);

    void setFilterText(const QString &text);

    void setCameras(const QnVirtualCameraResourceList &cameras);

    const QnVirtualCameraResourceList &cameras() const;

    void applyFilter();

    void sort(int column
        , Qt::SortOrder order);

    ///

    int rowCount(const QModelIndex &parent) const;

    int columnCount(const QModelIndex &parent) const;

    QVariant getData(const QModelIndex &index
        , int role);

private:
    typedef QHash<QString, QString> UniqIdToStringHash;

    const ResetOperationFunction m_beginResetModel;
    const ResetOperationFunction m_endResetModel;
    QnSearchBookmarksModel * const m_owner;

    QnCameraBookmarkSearchFilter m_filter;
    QnCameraBookmarksQueryPtr m_query;
    QnCameraBookmarkList m_bookmarks;
    QnVirtualCameraResourceList m_cameras;
    utils::QnCameraNamesWatcher m_cameraNamesWatcher;
};

///

QnSearchBookmarksModel::Impl::Impl(QnSearchBookmarksModel *owner
    , const ResetOperationFunction &beginResetModel
    , const ResetOperationFunction &endResetModel)
    : QObject(owner)
    , QnWorkbenchContextAware(owner)

    , m_beginResetModel(beginResetModel)
    , m_endResetModel(endResetModel)
    , m_owner(owner)

    , m_filter()
    , m_query()
    , m_bookmarks()
    , m_cameraNamesWatcher()
{
    m_filter.limit = kMaxVisibleRows;
    m_filter.orderBy = QnSearchBookmarksModel::defaultSortOrder();

    connect(&m_cameraNamesWatcher, &utils::QnCameraNamesWatcher::cameraNameChanged, this
        , [this](const QString &cameraUuid)
    {
        Q_UNUSED(cameraUuid);

        const auto topIndex = m_owner->index(0);
        const auto bottomIndex = m_owner->index(m_bookmarks.size() - 1);
        emit m_owner->dataChanged(topIndex, bottomIndex);
    });

    connect(qnCameraBookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved, this, [this](const QnUuid& bookmarkId) {
        int idx = qnIndexOf(m_bookmarks, [bookmarkId](const QnCameraBookmark& bookmark) {
            return bookmark.guid == bookmarkId;
        });
        if (idx < 0)
            return;
        m_beginResetModel();
        m_bookmarks.removeAt(idx);
        m_endResetModel();
    });
}

QnSearchBookmarksModel::Impl::~Impl()
{
}

void QnSearchBookmarksModel::Impl::setRange(qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs)
{
    m_filter.startTimeMs = utcStartTimeMs;
    m_filter.endTimeMs = utcFinishTimeMs;
}

void QnSearchBookmarksModel::Impl::setFilterText(const QString &text)
{
    m_filter.text = text;
}

const QnVirtualCameraResourceList &QnSearchBookmarksModel::Impl::cameras() const
{
    return m_cameras;
}

void QnSearchBookmarksModel::Impl::setCameras(const QnVirtualCameraResourceList &cameras)
{
    m_cameras = cameras;
}

void QnSearchBookmarksModel::Impl::applyFilter()
{
    //TODO: #GDM #bookmarks disable window instead of cleaning bookmarks - also add an option to cancel current request if long enough
    m_beginResetModel();
    m_bookmarks.clear();
    m_endResetModel();

    m_query = qnCameraBookmarksManager->createQuery();
    m_query->setFilter(m_filter);
    m_query->setCameras(m_cameras.toSet());
    m_query->executeRemoteAsync([this](bool success, const QnCameraBookmarkList &bookmarks)
    {
        if (!success)
            return;

        m_beginResetModel();
        m_bookmarks = bookmarks;
        m_endResetModel();
    });
}

void QnSearchBookmarksModel::Impl::sort(int column
    , Qt::SortOrder order)
{
    m_filter.orderBy = calculateSortOrder(column, order);
    if (m_bookmarks.size() < kMaxVisibleRows)   // All bookmarks should be loaded here
    {
        QnCameraBookmark::sortBookmarks(m_bookmarks, m_filter.orderBy);
        emit m_owner->dataChanged(m_owner->index(0, 0)
            , m_owner->index(m_bookmarks.size() - 1, kColumnsCount - 1));
    }
    else
        applyFilter();
}

int QnSearchBookmarksModel::Impl::rowCount(const QModelIndex &parent) const
{
    return (parent.isValid() ? 0 : m_bookmarks.size());
}

int QnSearchBookmarksModel::Impl::columnCount(const QModelIndex & /* parent */) const
{
    return Column::kColumnsCount;
}

QVariant QnSearchBookmarksModel::Impl::getData(const QModelIndex &index
    , int role)
{
    const int row = index.row();
    if ((row >= m_bookmarks.size()) || ((role != Qt::DisplayRole) && (role != Qn::CameraBookmarkRole)))
        return QVariant();

    const QnCameraBookmark &bookmark = m_bookmarks.at(row);

    if (role == Qn::CameraBookmarkRole)
        return QVariant::fromValue(bookmark);

    switch(index.column())
    {
    case kName:
        return bookmark.name;
    case kStartTime:
        return context()->instance<QnWorkbenchServerTimeWatcher>()->displayTime(bookmark.startTimeMs);
    case kLength:
        enum { kNoApproximation = -1};   /// Don't use approximation, because bookmark could have short lifetime
        return QTimeSpan(bookmark.durationMs).normalized().toApproximateString(kNoApproximation);
    case kTags:
        return QnCameraBookmark::tagsToString(bookmark.tags);
    case kCamera:
        return m_cameraNamesWatcher.getCameraName(bookmark.cameraId);
    default:
        return QString();
    }
}

///

QnSearchBookmarksModel::QnSearchBookmarksModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_impl(new Impl(this
        , [this]() { beginResetModel(); }
        , [this]() { endResetModel(); }))
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
    case Qn::BookmarkTags:
        return Column::kTags;
    case Qn::BookmarkCameraName:
        return Column::kCamera;
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Unhandled field");
        break;
    }
    return kInvalidSortingColumn;
}

void QnSearchBookmarksModel::applyFilter()
{
    m_impl->applyFilter();
}

void QnSearchBookmarksModel::setRange(qint64 utcStartTimeMs
    , qint64 utcFinishTimeMs)
{
    m_impl->setRange(utcStartTimeMs, utcFinishTimeMs);
}

void QnSearchBookmarksModel::setFilterText(const QString &text)
{
    m_impl->setFilterText(text);
}

void QnSearchBookmarksModel::setCameras(const QnVirtualCameraResourceList &cameras)
{
    m_impl->setCameras(cameras);
}

const QnVirtualCameraResourceList &QnSearchBookmarksModel::cameras() const
{
    return m_impl->cameras();
}

int QnSearchBookmarksModel::rowCount(const QModelIndex &parent) const
{
    return m_impl->rowCount(parent);
}

int QnSearchBookmarksModel::columnCount(const QModelIndex &parent) const
{
    return m_impl->columnCount(parent);
}

void QnSearchBookmarksModel::sort(int column, Qt::SortOrder order)
{
    m_impl->sort(column, order);
}

QModelIndex QnSearchBookmarksModel::index(int row, int column, const QModelIndex &parent) const
{
    return (hasIndex(row, column, parent) ? createIndex(row, column, nullptr) : QModelIndex());
}

QModelIndex QnSearchBookmarksModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

QVariant QnSearchBookmarksModel::data(const QModelIndex &index, int role) const
{
    return m_impl->getData(index, role);
}

QVariant QnSearchBookmarksModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole) || (section >= kColumnsCount))
        return QAbstractItemModel::headerData(section, orientation, role);

    static const QString kColumnHeaderNames[] = {tr("Name"), tr("Start time"), tr("Length"), tr("Tags"), tr("Camera")};
    return kColumnHeaderNames[section];
}
