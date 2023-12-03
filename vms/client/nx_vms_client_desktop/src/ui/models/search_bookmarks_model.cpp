// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_bookmarks_model.h"

#include <chrono>

#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <client/client_globals.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/algorithm/index_of.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/bookmarks/bookmark_utils.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/bookmark/bookmark_helpers.h>
#include <nx/vms/common/bookmark/bookmark_sort.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/time/formatter.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/camera/camera_names_watcher.h>

using std::chrono::milliseconds;

using namespace nx::vms::client::desktop;

namespace
{

static constexpr int kInvalidSortingColumn = -1;
static constexpr int kMaxVisibleRows = 10 * 1000;
static constexpr int kMaxNameLength = 64;
static constexpr int kMaxDescriptionLength = 128;

QnBookmarkSortOrder calculateSortOrder(int modelColumn, Qt::SortOrder order)
{
    const auto column =
        [modelColumn]() -> nx::vms::api::BookmarkSortField
        {
            switch(modelColumn)
            {
                case QnSearchBookmarksModel::kName:
                    return nx::vms::api::BookmarkSortField::name;
                case QnSearchBookmarksModel::kStartTime:
                    return nx::vms::api::BookmarkSortField::startTime;
                case QnSearchBookmarksModel::kLength:
                    return nx::vms::api::BookmarkSortField::duration;
                case QnSearchBookmarksModel::kCreationTime:
                    return nx::vms::api::BookmarkSortField::creationTime;
                case QnSearchBookmarksModel::kCreator:
                    return nx::vms::api::BookmarkSortField::creator;
                case QnSearchBookmarksModel::kTags:
                    return nx::vms::api::BookmarkSortField::tags;
                case QnSearchBookmarksModel::kDescription:
                    return nx::vms::api::BookmarkSortField::description;
                case QnSearchBookmarksModel::kCamera:
                    return nx::vms::api::BookmarkSortField::cameraName;
                default:
                    NX_ASSERT(false, "Wrong column");
                    return nx::vms::api::BookmarkSortField::startTime;
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

    void setCameras(const std::set<QnUuid>& cameras);

    const std::set<QnUuid>& cameras() const;

    void applyFilter();

    void sort(int column, Qt::SortOrder order);

    int rowCount(const QModelIndex& parent) const;

    int columnCount(const QModelIndex& parent) const;

    QVariant getData(const QModelIndex& index, int role) const;

    QDateTime displayTime(qint64 millisecondsSinceEpoch) const;

    void cancelUpdatingOperation();

private:
    int getBookmarkIndex(const QnUuid& bookmarkId) const;
    nx::utils::SharedGuardPtr startUpdateOperation();
    bool isUpdating() const;

private:
    using UniqIdToStringHash = QHash<QString, QString>;
    using UpdatingOperationWeakGuard = std::weak_ptr<nx::utils::SharedGuard>;

    UpdatingOperationWeakGuard m_updatingWeakGuard;
    QnCameraBookmarkSearchFilter m_filter;
    QnCameraBookmarksQueryPtr m_query;
    QnCameraBookmarkList m_bookmarks;
};

QnSearchBookmarksModelPrivate::QnSearchBookmarksModelPrivate(QnSearchBookmarksModel* owner):
    QObject(owner),
    QnWorkbenchContextAware(owner),
    q_ptr(owner)
{
    m_filter.limit = kMaxVisibleRows;
    m_filter.orderBy = QnSearchBookmarksModel::defaultSortOrder();

    connect(systemContext()->cameraNamesWatcher(), &QnCameraNamesWatcher::cameraNameChanged, this,
        [this](/*const QString &cameraUuid*/)
        {
            if (m_updatingWeakGuard.lock())
                return;

            Q_Q(QnSearchBookmarksModel);
            const auto topIndex = q->index(0);
            const auto bottomIndex = q->index(m_bookmarks.size() - 1);
            emit q->dataChanged(topIndex, bottomIndex);
        });

    // FIXME: #sivanov Actually we must listen for all system contexts here.
    auto bookmarksManager = appContext()->currentSystemContext()->cameraBookmarksManager();
    connect(bookmarksManager, &QnCameraBookmarksManager::bookmarkRemoved, this,
        [this](const QnUuid& bookmarkId)
        {
            if (m_updatingWeakGuard.lock())
                return;

            const auto index = getBookmarkIndex(bookmarkId);
            if (index < 0)
                return;

            Q_Q(QnSearchBookmarksModel);
            q->beginRemoveRows(QModelIndex(), index, index);
            m_bookmarks.erase(m_bookmarks.begin() + index);
            q->endRemoveRows();
        });

    connect(bookmarksManager, &QnCameraBookmarksManager::bookmarkUpdated, this,
        [this](const QnCameraBookmark& bookmark)
        {
            if (m_updatingWeakGuard.lock())
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
    // TODO: #sivanov Actualize used system context.
    const auto timeWatcher = appContext()->currentSystemContext()->serverTimeWatcher();
    return timeWatcher->displayTime(millisecondsSinceEpoch);
}

void QnSearchBookmarksModelPrivate::setRange(qint64 utcStartTimeMs, qint64 utcFinishTimeMs)
{
    m_filter.startTimeMs = milliseconds(utcStartTimeMs);
    m_filter.endTimeMs = milliseconds(utcFinishTimeMs);
}

void QnSearchBookmarksModelPrivate::setFilterText(const QString& text)
{
    m_filter.text = text;
}

const std::set<QnUuid>& QnSearchBookmarksModelPrivate::cameras() const
{
    return m_filter.cameras;
}

void QnSearchBookmarksModelPrivate::setCameras(const std::set<QnUuid>& cameras)
{
    m_filter.cameras = cameras;
}

void QnSearchBookmarksModelPrivate::cancelUpdatingOperation()
{
    if (!m_updatingWeakGuard.lock())
        return;

    m_bookmarks.clear();

    m_updatingWeakGuard.lock()->fire();
    m_updatingWeakGuard = UpdatingOperationWeakGuard();
}

bool QnSearchBookmarksModelPrivate::isUpdating() const
{
    return m_updatingWeakGuard.lock() != nullptr;
}

nx::utils::SharedGuardPtr QnSearchBookmarksModelPrivate::startUpdateOperation()
{
    if (isUpdating())
    {
        if (m_query && m_query->filter() == m_filter)
            return {}; //< Makes no sense to send a new request with the same filter data.

        cancelUpdatingOperation();
    }

    Q_Q(QnSearchBookmarksModel);

    q->beginResetModel();
    const auto guard = nx::utils::makeSharedGuard(
        [q]() { q->endResetModel(); });

    m_updatingWeakGuard = UpdatingOperationWeakGuard(guard);
    return guard;
}

void QnSearchBookmarksModelPrivate::applyFilter()
{
    const auto endUpdateOperationGuard = startUpdateOperation();
    if (!endUpdateOperationGuard)
        return;

    // FIXME: #sivanov Ensure request is sent to the correct system context (or to all required).
    auto bookmarksManager = appContext()->currentSystemContext()->cameraBookmarksManager();
    m_query = bookmarksManager->createQuery(m_filter);
    bookmarksManager->sendQueryRequest(m_query,
        [this, endUpdateOperationGuard]
        (bool success, int /*requestId*/, const QnCameraBookmarkList& bookmarks)
        {
            if (m_updatingWeakGuard.lock().get() != endUpdateOperationGuard.get())
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

        nx::vms::common::sortBookmarks(m_bookmarks, m_filter.orderBy.column,
            m_filter.orderBy.order, systemContext()->resourcePool());

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
    using namespace nx::vms::client::core;

    static const QSet<int> kAcceptedRolesSet =
        { Qt::DecorationRole, Qt::DisplayRole, Qt::ToolTipRole, CameraBookmarkRole };

    const int row = index.row();
    if (row >= m_bookmarks.size() || !kAcceptedRolesSet.contains(role))
        return QVariant();

    const QnCameraBookmark& bookmark = m_bookmarks.at(row);

    if (role == CameraBookmarkRole)
        return QVariant::fromValue(bookmark);


    if (role == Qt::DecorationRole)
    {
        if (index.column() == QnSearchBookmarksModel::kCamera)
            return qnResIconCache->icon(QnResourceIconCache::Camera);
        if (index.column() == QnSearchBookmarksModel::kCreator)
        {
            if (bookmark.creatorId.isNull())
                return QVariant();

            return bookmark.isCreatedBySystem()
                ? qnResIconCache->icon(QnResourceIconCache::CurrentSystem)
                : qnResIconCache->icon(QnResourceIconCache::User);
        }

        return QVariant();
    }

    // Return formatted time if display text is requested.
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
    {
        if (index.column() == QnSearchBookmarksModel::kStartTime)
            return nx::vms::time::toString(displayTime(bookmark.startTimeMs.count()));
        if (index.column() == QnSearchBookmarksModel::kCreationTime)
            return nx::vms::time::toString(displayTime(bookmark.creationTime().count()));
    }

    switch(index.column())
    {
        case QnSearchBookmarksModel::kName:
            return role == Qt::ToolTipRole
                ? bookmark.name
                : nx::utils::elideString(bookmark.name, kMaxNameLength);
        case QnSearchBookmarksModel::kStartTime:
            return displayTime(bookmark.startTimeMs.count());
        case QnSearchBookmarksModel::kCreationTime:
            return displayTime(bookmark.creationTime().count());
        case QnSearchBookmarksModel::kCreator:
            return getVisibleBookmarkCreatorName(bookmark, systemContext());
        case QnSearchBookmarksModel::kLength:
        {
            const auto duration = std::chrono::milliseconds(std::abs(bookmark.durationMs.count()));
            using HumanReadable = nx::vms::text::HumanReadable;
            return HumanReadable::timeSpan(duration,
                HumanReadable::DaysAndTime,
                HumanReadable::SuffixFormat::Full,
                lit(", "),
                HumanReadable::kNoSuppressSecondUnit);
        }
        case QnSearchBookmarksModel::kTags:
            return QnCameraBookmark::tagsToString(bookmark.tags);
        case QnSearchBookmarksModel::kDescription:
            return role == Qt::ToolTipRole
                ? bookmark.description
                : nx::utils::elideString(bookmark.description, kMaxDescriptionLength);
        case QnSearchBookmarksModel::kCamera:
            return systemContext()->cameraNamesWatcher()->getCameraName(bookmark.cameraId);
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
     return QnBookmarkSortOrder(nx::vms::api::BookmarkSortField::startTime, Qt::DescendingOrder);
}

int QnSearchBookmarksModel::sortFieldToColumn(nx::vms::api::BookmarkSortField field)
{
    switch (field)
    {
        case nx::vms::api::BookmarkSortField::name:
            return Column::kName;
        case nx::vms::api::BookmarkSortField::startTime:
            return Column::kStartTime;
        case nx::vms::api::BookmarkSortField::duration:
            return Column::kLength;
        case nx::vms::api::BookmarkSortField::creationTime:
            return Column::kCreationTime;
        case nx::vms::api::BookmarkSortField::creator:
            return Column::kCreator;
        case nx::vms::api::BookmarkSortField::tags:
            return Column::kTags;
        case nx::vms::api::BookmarkSortField::description:
            return Column::kDescription;
        case nx::vms::api::BookmarkSortField::cameraName:
            return Column::kCamera;
        default:
            NX_ASSERT(false, "Unhandled field");
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

void QnSearchBookmarksModel::setCameras(const std::set<QnUuid>& cameras)
{
    Q_D(QnSearchBookmarksModel);
    d->setCameras(cameras);
}

const std::set<QnUuid>& QnSearchBookmarksModel::cameras() const
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
        case kDescription:
            return tr("Description");
        default:
            NX_ASSERT(false, "Invalid column identifier");
            return QString();
    }
}
