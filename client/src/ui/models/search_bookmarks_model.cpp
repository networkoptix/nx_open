
#include "search_bookmarks_model.h"

#include <utils/common/qtimespan.h>
#include <core/resource/camera_bookmark.h>
#include <camera/camera_bookmarks_manager.h>

class QnSearchBookmarksModel::Impl : private QObject
{
public:
    Impl(QnSearchBookmarksModel *owner);
	
    ~Impl();

    ///
    void setDates(const QDate &start
        , const QDate &finish);

    void setFilter(const QString &text);

    void setCameras(const QnResourceList &cameras);
    
    void reload();

    void applyFilter();

    ///

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant getData(const QModelIndex &index
        , int role);

private:
    QnSearchBookmarksModel * const m_owner;
    QnCameraBookmarksManager * const m_bookmarksManager;
    QnCameraBookmarkList m_bookmarks;
    QnCameraBookmarksManager::FilterParameters m_filter;
};

///

QnSearchBookmarksModel::Impl::Impl(QnSearchBookmarksModel *owner)
    : QObject(owner)
    , m_owner(owner)
    , m_bookmarksManager(new QnCameraBookmarksManager(this))
    , m_bookmarks()
{
    QnCameraBookmark bookmark;
    m_bookmarks.push_back(bookmark);
}

QnSearchBookmarksModel::Impl::~Impl() 
{
}

void QnSearchBookmarksModel::Impl::setDates(const QDate &start
    , const QDate &finish)
{
    enum { kDayDurationMs = 24 * 60 * 60 * 1000 };
    m_filter.startTime = QDateTime(start).toMSecsSinceEpoch();
    m_filter.finishTime = QDateTime(finish).toMSecsSinceEpoch() + kDayDurationMs;
}

void QnSearchBookmarksModel::Impl::setFilter(const QString &text)
{
    m_filter.text = text;
}

void QnSearchBookmarksModel::Impl::setCameras(const QnResourceList &cameras)
{
    m_filter.cameras = cameras;
}
    
void QnSearchBookmarksModel::Impl::reload()
{
}

void QnSearchBookmarksModel::Impl::applyFilter()
{
    m_bookmarksManager->getBookmarksAsync(m_filter, [this](bool success, const QnCameraBookmarkList &bookmarks)
    {
        if (success && !bookmarks.empty())
        {
            m_bookmarks = bookmarks;
            emit m_owner->dataChanged(m_owner->createIndex(0, kColumnsCount)
                , m_owner->createIndex(m_bookmarks.size() - 1, kColumnsCount));
        }
        else
        {
            m_bookmarks = QnCameraBookmarkList();
            emit m_owner->dataChanged(QModelIndex(), QModelIndex());
        }
    });
}

int QnSearchBookmarksModel::Impl::rowCount(const QModelIndex &parent) const
{
    return m_bookmarks.size();
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
        enum { kNoApproximation = 0};   /// Don't use approzimation, because bookmark could have short lifetime 
        return QTimeSpan(bookmark.durationMs).normalized().toApproximateString(kNoApproximation);
    case kTags:
        return bookmark.tagsAsString();
    default:
        return QString();
    }
}

///

QnSearchBookmarksModel::QnSearchBookmarksModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_impl(new Impl(this))
{
}

QnSearchBookmarksModel::~QnSearchBookmarksModel()
{
}

void QnSearchBookmarksModel::reload()
{
    m_impl->reload();
}

void QnSearchBookmarksModel::applyFilter()
{
    m_impl->applyFilter();
}

void QnSearchBookmarksModel::setDates(const QDate &start
    , const QDate &finish)
{
    m_impl->setDates(start, finish);
}

void QnSearchBookmarksModel::setFilter(const QString &text)
{
    m_impl->setFilter(text);
}

void QnSearchBookmarksModel::setCameras(const QnResourceList &cameras)
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
}

QModelIndex QnSearchBookmarksModel::index(int row, int column, const QModelIndex &parent) const
{
    return QAbstractItemModel::createIndex(row, column, nullptr);
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
