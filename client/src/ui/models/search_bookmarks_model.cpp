
#include "search_bookmarks_model.h"

#include <utils/common/qtimespan.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource_management/resource_pool.h>
#include <camera/camera_bookmarks_manager.h>

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
{
public:
    Impl(QnSearchBookmarksModel *owner
        , const ResetOperationFunction &beginResetModel
        , const ResetOperationFunction &endResetModel);
	
    ~Impl();

    ///
    void setDates(const QDate &start
        , const QDate &finish);

    void setFilterText(const QString &text);

    void setCameras(const QnVirtualCameraResourceList &cameras);
    
    void applyFilter(bool clearBookmarksCache);

    void sort(int column
        , Qt::SortOrder order);

    ///

    int rowCount(const QModelIndex &parent) const;
    
    int columnCount(const QModelIndex &parent) const;

    QVariant getData(const QModelIndex &index
        , int role);

private:
    const QString &cameraNameFromId(const QString &id);

private:
    typedef std::map<QString, QString> UniqIdToStringMap;

    const ResetOperationFunction m_beginResetModel;
    const ResetOperationFunction m_endResetModel;
    QnSearchBookmarksModel * const m_owner;
    QnCameraBookmarksManager * const m_bookmarksManager;

    QnCameraBookmarkList m_bookmarks;
    QnVirtualCameraResourceList m_cameras;
    QnCameraBookmarkSearchFilter m_filter;
    UniqIdToStringMap m_camerasNames;

    int m_sortingColumn;
    Qt::SortOrder m_sortingOrder;
    QUuid m_searchRequestId;
};

///

QnSearchBookmarksModel::Impl::Impl(QnSearchBookmarksModel *owner
    , const ResetOperationFunction &beginResetModel
    , const ResetOperationFunction &endResetModel)
    : QObject(owner)
    , m_beginResetModel(beginResetModel)
    , m_endResetModel(endResetModel)
    , m_owner(owner)
    , m_bookmarksManager(new QnCameraBookmarksManager(this))

    , m_bookmarks()
    , m_filter()
    , m_camerasNames()

    , m_sortingColumn(kInvalidSortingColumn)
    , m_sortingOrder(Qt::AscendingOrder)
    , m_searchRequestId(QUuid::createUuid())
{
}

QnSearchBookmarksModel::Impl::~Impl() 
{
}

void QnSearchBookmarksModel::Impl::setDates(const QDate &start
    , const QDate &finish)
{
    static const QTime kStartOfTheDayTime = QTime(0, 0, 0, 0);
    static const QTime kEndOfTheDayTime = QTime(23, 59, 59, 999);
    m_filter.startTimeMs = QDateTime(start, kStartOfTheDayTime).toMSecsSinceEpoch();
    m_filter.endTimeMs = QDateTime(finish, kEndOfTheDayTime).toMSecsSinceEpoch();
}

void QnSearchBookmarksModel::Impl::setFilterText(const QString &text)
{
    m_filter.text = text;
}

void QnSearchBookmarksModel::Impl::setCameras(const QnVirtualCameraResourceList &cameras)
{
    m_cameras = cameras;
}

void QnSearchBookmarksModel::Impl::applyFilter(bool clearBookmarksCache)
{
    if (m_cameras.empty())
        m_cameras = qnResPool->getAllCameras(QnResourcePtr(), true);

    m_bookmarksManager->getBookmarksAsync(m_cameras, m_filter, m_searchRequestId
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
        sortBookmarks(m_bookmarks, order, [](const QnCameraBookmark &bookmark) { return bookmark.tagsAsString(); });
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
    
int QnSearchBookmarksModel::Impl::columnCount(const QModelIndex &parent) const
{ 
    return Column::kColumnsCount;
}

QVariant QnSearchBookmarksModel::Impl::getData(const QModelIndex &index
    , int role)
{
    const int row = index.row();
    if ((row >= m_bookmarks.size()) || (role != Qt::DisplayRole))
        return QVariant();

    const QnCameraBookmark &bookmark = m_bookmarks.at(row);
    switch(index.column())
    {
    case kName:
        return bookmark.name;
    case kStartTime:
        return QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs);
    case kLength:
        enum { kNoApproximation = 0};   /// Don't use approximation, because bookmark could have short lifetime 
        return QTimeSpan(bookmark.durationMs).normalized().toApproximateString(kNoApproximation);
    case kTags:
        return bookmark.tagsAsString();
    case kCamera:
        return cameraNameFromId(bookmark.cameraId);
    default:
        return QString();
    }
}

const QString &QnSearchBookmarksModel::Impl::cameraNameFromId(const QString &id)
{
    auto it = m_camerasNames.find(id);
    if (it == m_camerasNames.end())
        it = m_camerasNames.insert(std::make_pair(id, qnResPool->getResourceByUniqueId(id)->getName())).first;
    return it->second;
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

void QnSearchBookmarksModel::applyFilter(bool clearBookmarksCache)
{
    m_impl->applyFilter(clearBookmarksCache);
}

void QnSearchBookmarksModel::setDates(const QDate &start
    , const QDate &finish)
{
    m_impl->setDates(start, finish);
}

void QnSearchBookmarksModel::setFilterText(const QString &text)
{
    m_impl->setFilterText(text);
}

void QnSearchBookmarksModel::setCameras(const QnVirtualCameraResourceList &cameras)
{
    m_impl->setCameras(cameras);
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
        return Base::headerData(section, orientation, role);

    static const QString kColumnHeaderNames[] = {tr("Name"), tr("Start time"), tr("Length"), tr("Tags"), tr("Camera")};
    return kColumnHeaderNames[section];
}
