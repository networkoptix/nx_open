#include "right_panel_models_adapter.h"

#include <chrono>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>
#include <QtQml/QtQml>

#include <core/resource/resource.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/html.h>

#include <nx/utils/range_adapters.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/simple_motion_search_list_model.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>

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

    void setRead(int row);
    void setAutoClosePaused(int row, bool value);

private:
    void recreateSourceModel();
    void initDeadlines(int first, int last);
    void closeExpired();

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
};

// ------------------------------------------------------------------------------------------------
// RightPanelModelsAdapter

RightPanelModelsAdapter::RightPanelModelsAdapter(QObject* parent):
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

        default:
            return base_type::data(index, role);
    }
}

QHash<int, QByteArray> RightPanelModelsAdapter::roleNames() const
{
    auto roles = base_type::roleNames();
    roles.insert(Qt::ForegroundRole, "foregroundColor");
    roles.insert(Qn::ResourceRole, "previewResource");
    roles.insert(Qn::DescriptionTextRole, "description");
    roles.insert(Qn::DecorationPathRole, "decorationPath");
    roles.insert(Qn::TimestampTextRole, "timestamp");
    roles.insert(Qn::AdditionalTextRole, "additionalText");
    roles.insert(Qn::DisplayedResourceListRole, "resourceList");
    roles.insert(Qn::PreviewTimeRole, "previewTimestamp");
    roles.insert(Qn::ItemZoomRectRole, "previewCropRect");
    roles.insert(Qn::ForcePrecisePreviewRole, "precisePreview");
    roles.insert(Qn::RemovableRole, "isCloseable");
    roles.insert(Qn::AlternateColorRole, "isInformer");
    roles.insert(Qn::ProgressValueRole, "progressValue");
    // TODO: Preview stream
    return roles;
}

bool RightPanelModelsAdapter::removeRow(int row)
{
    return base_type::removeRows(row, 1);
}

void RightPanelModelsAdapter::setRead(int row)
{
    d->setRead(row);
}

void RightPanelModelsAdapter::setAutoClosePaused(int row, bool value)
{
    d->setAutoClosePaused(row, value);
}

void RightPanelModelsAdapter::registerQmlType()
{
    qmlRegisterType<RightPanelModelsAdapter>("nx.vms.client.desktop", 1, 0, "RightPanelModel");
}

// ------------------------------------------------------------------------------------------------
// RightPanelModelsAdapter::Private

RightPanelModelsAdapter::Private::Private(RightPanelModelsAdapter* q):
    q(q),
    m_autoCloseTimer(new QTimer())
{
    connect(q, &QAbstractListModel::modelAboutToBeReset, this,
        [this] { m_deadlines.clear(); });

    connect(q, &QAbstractListModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last)
        {
            for (int row = first; row <= last; ++row)
                m_deadlines.remove(this->q->index(row, 0));
        });

    connect(q, &QAbstractListModel::modelReset, this,
        [this] { initDeadlines(0, this->q->rowCount()); });

    connect(q, &QAbstractListModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last)
        {
            if (NX_ASSERT(!parent.isValid()))
                initDeadlines(first, last);
        });

    connect(m_autoCloseTimer.get(), &QTimer::timeout, this, &Private::closeExpired);
    m_autoCloseTimer->start(1s);
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

    if (oldSourceModel && oldSourceModel->parent() == this)
        delete oldSourceModel;

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
}

void RightPanelModelsAdapter::Private::initDeadlines(int first, int last)
{
    for (int row = first; row <= last; ++row)
    {
        const auto index = q->index(row, 0);
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

void RightPanelModelsAdapter::Private::setRead(int row)
{
    const auto iter = m_deadlines.find(q->index(row, 0));
    if (iter != m_deadlines.cend())
        iter->timer.setRemainingTime(kVisibleAutoCloseDelay);
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

} // namespace nx::vms::client::desktop
