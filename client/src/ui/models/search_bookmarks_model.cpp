
#include "search_bookmarks_model.h"

#include <utils/common/qtimespan.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>
#include <camera/camera_bookmarks_manager.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

namespace
{
    typedef std::function<void ()> ResetOperationFunction;

    enum { kInvalidSortingColumn = -1 };

    template<typename PredType
        , typename Accessor>
    void sortImpl(QnCameraBookmarkList &list
        , const Accessor &accessor)
    {
        PredType pred;
        std::sort(list.begin(), list.end()
            , [&accessor, pred](const QnCameraBookmark &first, const QnCameraBookmark &second)
        {
            return pred(accessor(first), accessor(second));
        });
    }

    template<typename Accessor>
    void sortBookmarks(QnCameraBookmarkList &list
        , Qt::SortOrder order
        , const Accessor &accessor)
    {
        typedef decltype(accessor(QnCameraBookmark())) AccessorResultType;

        if (order == Qt::AscendingOrder)
        {
            sortImpl<std::less<AccessorResultType>>(list, accessor);
        }
        else
        {
            sortImpl<std::greater<AccessorResultType>>(list, accessor);
        }
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
    QString cameraNameFromId(const QString &id);

private:
    typedef QHash<QString, QString> UniqIdToStringHash;

    const ResetOperationFunction m_beginResetModel;
    const ResetOperationFunction m_endResetModel;
    QnSearchBookmarksModel * const m_owner;
    QnCameraBookmarksManager * const m_bookmarksManager;

    QnCameraBookmarkList m_bookmarks;
    QnVirtualCameraResourceList m_cameras;
    QnCameraBookmarkSearchFilter m_filter;
    UniqIdToStringHash m_camerasNames;

    int m_sortingColumn;
    Qt::SortOrder m_sortingOrder;
    QUuid m_searchRequestId;
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
    , m_bookmarksManager(qnCameraBookmarksManager)

    , m_bookmarks()
    , m_filter()
    , m_camerasNames()

    , m_sortingColumn(kStartTime)
    , m_sortingOrder(Qt::DescendingOrder)
    , m_searchRequestId(QUuid::createUuid())
{
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
    m_beginResetModel();
    m_bookmarks.clear();
    m_endResetModel();

    m_bookmarksManager->getBookmarksAsync(m_cameras.toSet(), m_filter
        , [this](bool success, const QnCameraBookmarkList &bookmarks)
    {
        m_beginResetModel();
        if (!bookmarks.empty())
        {
            m_bookmarks = bookmarks;
            sort(m_sortingColumn, m_sortingOrder);
        }
        else
        {
            m_bookmarks = QnCameraBookmarkList();
        }
        if (!success)
        {
            /// TODO: ynikitenkov Add warning dialog - not all data loaded
        }
        m_endResetModel();
    });
}

void QnSearchBookmarksModel::Impl::sort(int column
    , Qt::SortOrder order)
{
    switch(column)
    {
    case kName:
        sortBookmarks(m_bookmarks, order, [](const QnCameraBookmark &bookmark) { return bookmark.name; });
        break;
    case kStartTime:
        sortBookmarks(m_bookmarks, order, [](const QnCameraBookmark &bookmark) { return bookmark.startTimeMs; });
        break;
    case kLength:
        sortBookmarks(m_bookmarks, order, [](const QnCameraBookmark &bookmark) { return bookmark.durationMs; });
        break;
    case kTags:
        sortBookmarks(m_bookmarks, order, [](const QnCameraBookmark &bookmark) { return QnCameraBookmark::tagsToString(bookmark.tags); });
        break;
    case kCamera:
        sortBookmarks(m_bookmarks, order, [this](const QnCameraBookmark &bookmark) { return cameraNameFromId(bookmark.cameraId); });
        break;
    default:
        return;
    }

    m_sortingColumn = column;
    m_sortingOrder = order;

    emit m_owner->dataChanged(m_owner->index(0, 0)
        , m_owner->index(m_bookmarks.size() - 1, kColumnsCount - 1));
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
        enum { kNoApproximation = 0};   /// Don't use approximation, because bookmark could have short lifetime
        return QTimeSpan(bookmark.durationMs).normalized().toApproximateString(kNoApproximation);
    case kTags:
        return QnCameraBookmark::tagsToString(bookmark.tags);
    case kCamera:
        return cameraNameFromId(bookmark.cameraId);
    default:
        return QString();
    }
}

QString QnSearchBookmarksModel::Impl::cameraNameFromId(const QString &id)
{
    const auto it = m_camerasNames.find(id);
    if (it != m_camerasNames.end())
        return *it;

    const auto cameraResource = qnResPool->getResourceByUniqueId<QnVirtualCameraResource>(id);
    if (!cameraResource)
        return QString();

    connect(cameraResource.data(), &QnVirtualCameraResource::nameChanged, this
        , [this, id, cameraResource](const QnResourcePtr & /* resource */)
    {
        static const auto kCameraRoleToUpdate = QVector<int>(1, kCamera);

        const auto newName = cameraResource->getName();
        m_camerasNames[id] = newName;

        const auto topIndex = m_owner->index(0);
        const auto bottomIndex = m_owner->index(m_bookmarks.size() - 1);
        m_owner->dataChanged(topIndex, bottomIndex, kCameraRoleToUpdate);
    });

    const auto cameraName = cameraResource->getName();
    m_camerasNames[id] = cameraName;
    return cameraName;
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