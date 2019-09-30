#include "right_panel_models_adapter.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <limits>
#include <memory>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtQml/QtQml>

#include <core/resource/media_resource.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/common/html.h>

#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>
#include <nx/vms/client/desktop/image_providers/resource_thumbnail_provider.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

/*
 * Tiles can have optional timed auto-close mode.
 * When such tile is first created, expiration timer is set to kInvisibleAutoCloseDelay.
 * Every time the tile becomes visible, remaining time is reset to kVisibleAutoCloseDelay.
 * When the tile becomes invisible, remaining time is not changed.
 * When the tile is hovered, the timer is ignored.
 * When the tile stops being hovered, remaining time is reset to kVisibleAutoCloseDelay.
 */
static constexpr milliseconds kVisibleAutoCloseDelay = 20s;
static constexpr milliseconds kInvisibleAutoCloseDelay = 80s;

static constexpr milliseconds kQueuedFetchDelay = 50ms;

static constexpr int kDefaultThumbnailWidth = 224;

milliseconds previewLoadDelay()
{
    return ini().tilePreviewLoadDelayOverrideMs < 1
        ? 1ms
        : milliseconds(ini().tilePreviewLoadDelayOverrideMs);
}

// TODO: #vkutin Implement "NO DATA" preview reloading!
// Right now it has deadline timers but no polling of any sort.

// Delay after which preview may be requested again after receiving "NO DATA".
milliseconds previewReloadDelay()
{
    return seconds(ini().rightPanelPreviewReloadDelay);
}

milliseconds sequentialPreviewLoadDelay()
{
    return milliseconds(ini().tilePreviewLoadIntervalMs);
}

bool sequentialPreviewLoad()
{
    return sequentialPreviewLoadDelay() > 0ms;
}

int maximumThumbnailWidth()
{
    return ini().rightPanelMaxThumbnailWidth;
}

bool isBusy(Qn::ThumbnailStatus status)
{
    return status == Qn::ThumbnailStatus::Loading || status == Qn::ThumbnailStatus::Refreshing;
}

qlonglong providerId(ResourceThumbnailProvider* provider)
{
    return qlonglong(provider);
}

QHash<intptr_t, ResourceThumbnailProvider*>& previewsById()
{
    static QHash<intptr_t, ResourceThumbnailProvider*> data;
    return data;
}

} // namespace

class RightPanelModelsAdapter::Private: public QObject
{
    RightPanelModelsAdapter* const q;

public:
    Private(RightPanelModelsAdapter* q);

    QnWorkbenchContext* context() const { return m_context; }
    void setContext(QnWorkbenchContext* value);

    RightPanel::Tab type() const { return m_type; }
    void setType(RightPanel::Tab value);

    void setAutoClosePaused(int row, bool value);

    void requestFetch();

    QString previewId(int row) const;
    qreal previewAspectRatio(int row) const;
    RightPanel::PreviewState previewState(int row) const;

    bool isVisible(const QModelIndex& index) const;
    bool setVisible(const QModelIndex& index, bool value);

    AbstractSearchListModel* searchModel();

    enum Role
    {
        PreviewIdRole = Qn::ItemDataRoleCount,
        PreviewStateRole,
        PreviewAspectRatioRole,
        ItemIsVisibleRole
    };

private:
    void recreateSourceModel();
    void createDeadlines(int first, int count);
    void closeExpired();
    void createPreviewProviders(int first, int count);
    void updatePreviewProvider(int row);
    void requestPreview(int row);
    void loadNextPreview();
    void loadPreviews();

private:
    QnWorkbenchContext* m_context = nullptr;
    RightPanel::Tab m_type = RightPanel::Tab::invalid;

    struct Deadline
    {
        QDeadlineTimer timer;
        bool paused = false;
    };

    QHash<QPersistentModelIndex, Deadline> m_deadlines;
    const QScopedPointer<QTimer> m_autoCloseTimer;

    nx::utils::PendingOperation m_fetchOperation;
    nx::utils::PendingOperation m_previewLoadOperation;

    QSet<QPersistentModelIndex> m_visibleItems;

    // Previews.
    using PreviewProviderPtr = std::unique_ptr<ResourceThumbnailProvider>;
    struct PreviewData
    {
        PreviewProviderPtr provider;
        QDeadlineTimer reloadTimer;
    };

    std::deque<PreviewData> m_previews;
    bool requiresLoading(const PreviewData& data) const;
};

// ------------------------------------------------------------------------------------------------
// RightPanelModelsAdapter

RightPanelModelsAdapter::RightPanelModelsAdapter(QObject* parent) :
    base_type(parent),
    d(new Private(this))
{
}

RightPanelModelsAdapter::~RightPanelModelsAdapter()
{
    // Required here for forward-declared pointer destruction.
}

QnWorkbenchContext* RightPanelModelsAdapter::context() const
{
    return d->context();
}

void RightPanelModelsAdapter::setContext(QnWorkbenchContext* value)
{
    d->setContext(value);
}

RightPanel::Tab RightPanelModelsAdapter::type() const
{
    return d->type();
}

void RightPanelModelsAdapter::setType(RightPanel::Tab value)
{
    d->setType(value);
}

QVariant RightPanelModelsAdapter::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(checkIndex(index)))
        return {};

    switch (role)
    {
        case Qn::ResourceRole:
        {
            const auto resource = base_type::data(index, role).value<QnResourcePtr>();
            if (!resource)
                return {};

            QQmlEngine::setObjectOwnership(resource.get(), QQmlEngine::CppOwnership);
            return QVariant::fromValue(resource.get());
        }

        case Qn::DisplayedResourceListRole:
        {
            const auto data = base_type::data(index, role);
            QStringList result = data.value<QStringList>();

            if (result.empty())
            {
                auto resources = data.value<QnResourceList>();
                for (const auto& resource: resources)
                    result.push_back(htmlBold(resource->getName()));
            }

            return result;
        }

        case Private::ItemIsVisibleRole:
            return d->isVisible(index);

        case Private::PreviewIdRole:
            return d->previewId(index.row());

        case Private::PreviewStateRole:
            return QVariant::fromValue(d->previewState(index.row()));

        case Private::PreviewAspectRatioRole:
            return d->previewAspectRatio(index.row());

        default:
            return base_type::data(index, role);
    }
}

bool RightPanelModelsAdapter::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Private::ItemIsVisibleRole || !NX_ASSERT(checkIndex(index)))
        return false;

    return d->setVisible(index, value.toBool());
}

QHash<int, QByteArray> RightPanelModelsAdapter::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[Qt::ForegroundRole] = "foregroundColor";
    roles[Qn::ResourceRole] = "previewResource";
    roles[Qn::DescriptionTextRole] = "description";
    roles[Qn::DecorationPathRole] = "decorationPath";
    roles[Qn::TimestampTextRole] = "timestamp";
    roles[Qn::AdditionalTextRole] = "additionalText";
    roles[Qn::DisplayedResourceListRole] = "resourceList";
    roles[Qn::PreviewTimeRole] = "previewTimestamp";
    roles[Qn::ItemZoomRectRole] = "previewCropRect";
    roles[Qn::RemovableRole] = "isCloseable";
    roles[Qn::AlternateColorRole] = "isInformer";
    roles[Qn::ProgressValueRole] = "progressValue";
    roles[Private::ItemIsVisibleRole] = "visible";
    roles[Private::PreviewIdRole] = "previewId";
    roles[Private::PreviewStateRole] = "previewState";
    roles[Private::PreviewAspectRatioRole] = "previewAspectRatio";
    return roles;
}

bool RightPanelModelsAdapter::removeRow(int row)
{
    return base_type::removeRows(row, 1);
}

void RightPanelModelsAdapter::setAutoClosePaused(int row, bool value)
{
    d->setAutoClosePaused(row, value);
}

void RightPanelModelsAdapter::setFetchDirection(RightPanel::FetchDirection value)
{
    if (auto model = d->searchModel())
        model->setFetchDirection(value);
}

void RightPanelModelsAdapter::setLivePaused(bool value)
{
    if (auto model = d->searchModel())
        model->setLivePaused(value);
}

void RightPanelModelsAdapter::requestFetch()
{
    d->requestFetch();
}

void RightPanelModelsAdapter::registerQmlType()
{
    qmlRegisterType<RightPanelModelsAdapter>("nx.vms.client.desktop", 1, 0, "RightPanelModel");
}

// ------------------------------------------------------------------------------------------------
// RightPanelModelsAdapter::Private

RightPanelModelsAdapter::Private::Private(RightPanelModelsAdapter* q) :
    q(q),
    m_autoCloseTimer(new QTimer())
{
    connect(q, &QAbstractListModel::modelAboutToBeReset, this,
        [this]
        {
            m_deadlines.clear();
            m_previews.clear();
        });

    connect(q, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int row = first; row <= last; ++row)
                m_deadlines.remove(this->q->index(row, 0));

            m_previews.erase(m_previews.begin() + first, m_previews.begin() + (last + 1));
        });

    connect(q, &QAbstractListModel::modelReset, this,
        [this]
        {
            const auto count = this->q->rowCount();
            createDeadlines(0, count);
            createPreviewProviders(0, count);
        });

    connect(q, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            if (!NX_ASSERT(!parent.isValid()))
                return;

            const auto count = last - first + 1;
            createDeadlines(first, count);
            createPreviewProviders(first, count);
        });

    connect(m_autoCloseTimer.get(), &QTimer::timeout, this, &Private::closeExpired);
    m_autoCloseTimer->start(1s);

    m_fetchOperation.setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_fetchOperation.setInterval(kQueuedFetchDelay);
    m_fetchOperation.setCallback(
        [this]()
        {
            if (const auto model = searchModel(); model && model->canFetchMore())
                model->fetchMore();
        });

    m_previewLoadOperation.setFlags(sequentialPreviewLoad()
        ? nx::utils::PendingOperation::FireImmediately
        : nx::utils::PendingOperation::FireOnlyWhenIdle);

    m_previewLoadOperation.setInterval(sequentialPreviewLoad()
        ? sequentialPreviewLoadDelay()
        : previewLoadDelay());

    if (sequentialPreviewLoad())
        m_previewLoadOperation.setCallback([this]() { loadNextPreview(); });
    else
        m_previewLoadOperation.setCallback([this]() { loadPreviews(); });
}

void RightPanelModelsAdapter::Private::setContext(QnWorkbenchContext* value)
{
    if (m_context == value)
        return;

    m_context = value;
    recreateSourceModel();
    emit q->contextChanged();
}

void RightPanelModelsAdapter::Private::setType(RightPanel::Tab value)
{
    if (m_type == value)
        return;

    m_type = value;
    recreateSourceModel();
    emit q->typeChanged();
}

void RightPanelModelsAdapter::Private::recreateSourceModel()
{
    const auto oldSourceModel = q->sourceModel();
    q->setSourceModel(nullptr);

    if (oldSourceModel)
    {
        if (oldSourceModel->parent() == this)
            delete oldSourceModel;
        else
            oldSourceModel->disconnect(q);
    }

    if (!m_context)
        return;

    switch (m_type)
    {
        case RightPanel::Tab::invalid:
            break;

        case RightPanel::Tab::notifications:
            q->setSourceModel(new NotificationTabModel(m_context, this));
            break;

        case RightPanel::Tab::motion:
            q->setSourceModel(new SimpleMotionSearchListModel(m_context, this));
            break;

        case RightPanel::Tab::bookmarks:
            q->setSourceModel(new BookmarkSearchListModel(m_context, this));
            break;

        case RightPanel::Tab::events:
            q->setSourceModel(new EventSearchListModel(m_context, this));
            break;

        case RightPanel::Tab::analytics:
            q->setSourceModel(new AnalyticsSearchListModel(m_context, this));
            break;
    }

    if (auto model = searchModel())
    {
        model->cameraSet()->setAllCameras();

        connect(model, &AbstractSearchListModel::dataNeeded,
            q, &RightPanelModelsAdapter::dataNeeded);
    }
}

void RightPanelModelsAdapter::Private::createDeadlines(int first, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const auto index = q->index(first + i, 0);
        const auto closeable = index.data(Qn::RemovableRole).toBool();
        const auto timeout = index.data(Qn::TimeoutRole).value<milliseconds>();

        if (closeable && timeout > 0ms)
            m_deadlines[index].timer = QDeadlineTimer(kInvisibleAutoCloseDelay);
    }
};

void RightPanelModelsAdapter::Private::closeExpired()
{
    QList<QPersistentModelIndex> expired;
    const int oldDeadlineCount = m_deadlines.size();

    for (const auto& [index, deadline]: nx::utils::keyValueRange(m_deadlines))
    {
        if (!deadline.paused && deadline.timer.hasExpired() && index.isValid())
            expired.push_back(index);
    }

    if (expired.empty())
        return;

    for (const auto index: expired)
        q->removeRow(index.row());

    NX_VERBOSE(q, "Expired %1 tiles", expired.size());
    NX_ASSERT(expired.size() == (oldDeadlineCount - m_deadlines.size()));
}

void RightPanelModelsAdapter::Private::createPreviewProviders(int first, int count)
{
    if (!NX_ASSERT(first >= 0 && first <= m_previews.size()))
        return;

    std::vector<PreviewData> inserted(count);
    m_previews.insert(m_previews.begin() + first,
        std::make_move_iterator(inserted.begin()), std::make_move_iterator(inserted.end()));

    for (int i = 0; i < count; ++i)
        updatePreviewProvider(first + i);
}

void RightPanelModelsAdapter::Private::updatePreviewProvider(int row)
{
    if (!NX_ASSERT(row >= 0 && row < m_previews.size() && m_previews.size() == q->rowCount()))
        return;

    const auto index = q->index(row, 0);
    const auto previewResource = index.data(Qn::ResourceRole).value<QnResource*>();
    if (!previewResource || !previewResource->hasFlags(Qn::video))
    {
        m_previews[row] = {};
        return;
    }

    const auto previewTime = index.data(Qn::PreviewTimeRole).value<microseconds>();
    const auto previewCropRect = index.data(Qn::ItemZoomRectRole).value<QRectF>();
    const auto thumbnailWidth = previewCropRect.isEmpty()
        ? kDefaultThumbnailWidth
        : qMin<int>(kDefaultThumbnailWidth / previewCropRect.width(), maximumThumbnailWidth());

    const bool precisePreview = !previewCropRect.isEmpty()
        || index.data(Qn::ForcePrecisePreviewRole).toBool();

    nx::api::ResourceImageRequest request;
    request.resource = previewResource->toSharedPointer();
    request.usecSinceEpoch =
        previewTime.count() > 0 ? previewTime.count() : nx::api::ImageRequest::kLatestThumbnail;
    request.rotation = nx::api::ImageRequest::kDefaultRotation;
    request.size = QSize(thumbnailWidth, 0);
    request.imageFormat = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::source;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;
    request.streamSelectionMode = index.data(Qn::PreviewStreamSelectionRole)
        .value<nx::api::ImageRequest::StreamSelectionMode>();

    auto& previewProvider = m_previews[row].provider;
    if (!previewProvider || request.resource != previewProvider->requestData().resource)
    {
        previewProvider.reset(new ResourceThumbnailProvider(request));
        const auto id = providerId(previewProvider.get());
        previewsById()[id] = previewProvider.get();
        connect(previewProvider.get(), &QObject::destroyed, this,
            [id]() { previewsById().remove(id); });
    }
    else
    {
        const auto oldRequest = previewProvider->requestData();
        const bool forceUpdate = oldRequest.usecSinceEpoch != request.usecSinceEpoch
            || oldRequest.streamSelectionMode != request.streamSelectionMode;

        previewProvider->setRequestData(request, forceUpdate);
    }

    if (requiresLoading(m_previews[row]))
        m_previewLoadOperation.requestOperation();
}

QString RightPanelModelsAdapter::Private::previewId(int row) const
{
    const auto& provider = m_previews[row].provider;
    return provider ? QString::number(providerId(provider.get())) : QString();
}

qreal RightPanelModelsAdapter::Private::previewAspectRatio(int row) const
{
    const auto& provider = m_previews[row].provider;
    const QSizeF size = provider ? provider->sizeHint() : QSizeF();
    return (size.height()) > 0 ? (size.width() / size.height()) : 1.0;
}

RightPanel::PreviewState RightPanelModelsAdapter::Private::previewState(int row) const
{
    if (const auto& provider = m_previews[row].provider)
    {
        if (!provider->image().isNull())
            return RightPanel::PreviewState::ready;

        switch (provider->status())
        {
            case Qn::ThumbnailStatus::Invalid:
                return RightPanel::PreviewState::initial;

            case Qn::ThumbnailStatus::Loading:
            case Qn::ThumbnailStatus::Refreshing:
                return RightPanel::PreviewState::busy;

            case Qn::ThumbnailStatus::Loaded:
                return RightPanel::PreviewState::ready;

            case Qn::ThumbnailStatus::NoData:
                return RightPanel::PreviewState::missing;
        }
    }

    return RightPanel::PreviewState::missing;
}

void RightPanelModelsAdapter::Private::setAutoClosePaused(int row, bool value)
{
    const auto iter = m_deadlines.find(q->index(row, 0));
    if (iter != m_deadlines.cend())
    {
        iter->paused = value;
        if (!value)
            iter->timer.setRemainingTime(kVisibleAutoCloseDelay);
    }
}

bool RightPanelModelsAdapter::Private::isVisible(const QModelIndex& index) const
{
    return m_visibleItems.contains(index);
}

bool RightPanelModelsAdapter::Private::setVisible(const QModelIndex& index, bool value)
{
    if (!NX_ASSERT(index.isValid()) || isVisible(index) == value)
        return false;

    if (value)
    {
        m_visibleItems.insert(index);

        if (requiresLoading(m_previews[index.row()]))
            m_previewLoadOperation.requestOperation();

        const auto iter = m_deadlines.find(q->index(index.row(), 0));
        if (iter != m_deadlines.cend())
            iter->timer.setRemainingTime(kVisibleAutoCloseDelay);
    }
    else
    {
        m_visibleItems.remove(index);
    }

    return true;
}

// This function is called only in non-sequential preview load mode.
void RightPanelModelsAdapter::Private::loadPreviews()
{
    NX_VERBOSE(q, "Load required previews for visible items...");

    for (const auto& index: m_visibleItems)
    {
        if (NX_ASSERT(index.isValid()))
            requestPreview(index.row());
    }
}

// This function is called only in sequential preview load mode.
void RightPanelModelsAdapter::Private::loadNextPreview()
{
    NX_VERBOSE(q, "Load next required preview...");

    int row = std::numeric_limits<int>::max();
    for (const auto& index: m_visibleItems)
    {
        if (NX_ASSERT(index.isValid())
            && index.row() < row
            && requiresLoading(m_previews[index.row()]))
        {
            row = index.row();
        }
    }

    if (row == std::numeric_limits<int>::max())
        return;

    requestPreview(row);
    m_previewLoadOperation.requestOperation(); //< Queue the next one.
}

void RightPanelModelsAdapter::Private::requestPreview(int row)
{
    auto& preview = m_previews[row];
    if (!requiresLoading(preview))
        return;

    const QPersistentModelIndex index(q->index(row, 0));
    auto connection = std::make_shared<QMetaObject::Connection>();

    *connection = connect(preview.provider.get(), &ImageProvider::statusChanged, this,
        [this, connection, index](Qn::ThumbnailStatus status)
        {
            if (isBusy(status))
                return;

            QObject::disconnect(*connection);
            if (!index.isValid())
                return;

            static const QVector<int> roles({
                Private::PreviewIdRole, Private::PreviewStateRole, Private::PreviewAspectRatioRole});

            emit q->dataChanged(index, index, roles);
        });

    NX_VERBOSE(q, "Requesting preview load for row=%1", row);

    preview.provider->loadAsync();
    preview.reloadTimer.setRemainingTime(previewReloadDelay());
}

bool RightPanelModelsAdapter::Private::requiresLoading(const PreviewData& data) const
{
    if (!data.provider)
        return false;

    const auto status = data.provider->status();
    return status == Qn::ThumbnailStatus::Invalid
        || (status == Qn::ThumbnailStatus::NoData && data.reloadTimer.hasExpired());
}

void RightPanelModelsAdapter::Private::requestFetch()
{
    m_fetchOperation.requestOperation();
}

AbstractSearchListModel* RightPanelModelsAdapter::Private::searchModel()
{
    return qobject_cast<AbstractSearchListModel*>(q->sourceModel());
}

// ------------------------------------------------------------------------------------------------

QImage RightPanelImageProvider::requestImage(
    const QString& id, QSize* size, const QSize& /*requestedSize*/)
{
    const auto provider = previewsById().value(id.toLongLong());
    return provider ? provider->image() : QImage();
}

} // namespace nx::vms::client::desktop
