// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_search_widget.h"

#include <chrono>

#include <QtCore/QTimer>

#include <core/resource/camera_resource.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/desktop/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

using nx::vms::client::core::AccessController;

namespace {

static constexpr milliseconds kVisiblePeriodUpdateInterval = 15s;
static constexpr milliseconds kDelayAfterViewUpdate = 1s;
static constexpr milliseconds kUpdateTimerResolution = 500ms;

core::SvgIconColorer::ThemeSubstitutions kPlaceholderTheme = {
    {QnIcon::Normal, {.primary = "dark16"}}};

NX_DECLARE_COLORIZED_ICON(
    kBookmarkPlaceholderIcon, "64x64/Outline/nobookmarks.svg", kPlaceholderTheme)

} // namespace

struct BookmarkSearchWidget::Private
{
    Q_DECLARE_TR_FUNCTIONS(BookmarkSearchWidget::Private)

public: //< Overrides 'private:' in macro above.
    BookmarkSearchListModel* const model;

    nx::utils::ElapsedTimer sinceLastRequest = {};
    nx::utils::ElapsedTimer sinceViewUpdated = {};

    nx::utils::ScopedConnections connections = {};

    // Request bookmarks in the visible range to check if something is to be added in between.
    void updateVisiblePeriod(BookmarkSearchWidget* q)
    {
        if (q->isVisible()
            && sinceLastRequest.hasExpired(kVisiblePeriodUpdateInterval)
            && sinceViewUpdated.hasExpired(kDelayAfterViewUpdate))
        {

            sinceLastRequest.restart();
            model->dynamicUpdate(q->currentCentralPointMs().count());
        }
    }
};

BookmarkSearchWidget::BookmarkSearchWidget(WindowContext* context, QWidget* parent):
    base_type(context, new BookmarkSearchListModel(context->system()), parent),
    d(new Private{qobject_cast<BookmarkSearchListModel*>(model())})
{
    NX_CRITICAL(d->model);

    setPlaceholderPixmap(qnSkin->icon(kBookmarkPlaceholderIcon).pixmap(64, 64));
    commonSetup()->setCameraSelection(core::EventSearch::CameraSelection::layout);

    // Track remote bookmark changes by polling.
    QPointer<QTimer> updateTimer = new QTimer(this);
    updateTimer->setInterval(kUpdateTimerResolution);
    connect(updateTimer.data(), &QTimer::timeout, this,
        [this]() { d->updateVisiblePeriod(this); });

    installEventHandler(this, {QEvent::Show, QEvent::Hide}, this,
        [updateTimer](QObject* /*watched*/, QEvent* event)
        {
            if (event->type() == QEvent::Show)
                updateTimer->start();
            else
                updateTimer->stop();
        });

    // Signals that can potentially be emitted during this widget destruction must be
    // disconnected in ~Private, so store the connections in d->connections.

    d->connections << connect(model(), &core::AbstractSearchListModel::isOnlineChanged,
        this, &BookmarkSearchWidget::updateAllowance);

    d->connections << connect(view(), &EventRibbon::visibleRangeChanged, this,
        [this]() { d->sinceViewUpdated.restart(); });
}

BookmarkSearchWidget::~BookmarkSearchWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void BookmarkSearchWidget::resetFilters()
{
    base_type::resetFilters();
    commonSetup()->setCameraSelection(core::EventSearch::CameraSelection::layout);
}

QString BookmarkSearchWidget::placeholderText(bool /*constrained*/) const
{
    return makePlaceholderText(tr("No bookmarks"), tr("Select a time span on the timeline and "
        "right-click the highlighted section to create a bookmark"));
}

QString BookmarkSearchWidget::itemCounterText(int count) const
{
    return tr("%n bookmarks", "", count);
}

bool BookmarkSearchWidget::calculateAllowance() const
{
    return model()->isOnline() && system()->accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewBookmarks);
}

} // namespace nx::vms::client::desktop
