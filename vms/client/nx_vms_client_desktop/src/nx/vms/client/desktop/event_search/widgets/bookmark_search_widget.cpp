#include "bookmark_search_widget.h"

 #include <chrono>

#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

namespace {

static constexpr milliseconds kVisiblePeriodUpdateInterval = 15s;
static constexpr milliseconds kDelayAfterViewUpdate = 1s;
static constexpr milliseconds kUpdateTimerResolution = 500ms;

} // namespace

struct BookmarkSearchWidget::Private
{
    BookmarkSearchListModel* const model;

    nx::utils::ElapsedTimer sinceLastRequest;
    nx::utils::ElapsedTimer sinceViewUpdated;

    void updateVisiblePeriod(BookmarkSearchWidget* q)
    {
        if (q->isVisible()
            && sinceLastRequest.hasExpired(kVisiblePeriodUpdateInterval)
            && sinceViewUpdated.hasExpired(kDelayAfterViewUpdate))
        {
            const auto range = q->view()->visibleRange();
            if (range.isEmpty())
                return;

            QnTimePeriod period;
            if (range.upper() >= q->view()->count())
            {
                period.setStartTime(0ms);
            }
            else
            {
                period.setStartTime(duration_cast<milliseconds>(model->index(range.upper() - 1)
                    .data(Qn::TimestampRole).value<microseconds>()));
            }

            if (range.lower() == 0)
            {
                period.durationMs = QnTimePeriod::kInfiniteDuration;
            }
            else
            {
                const auto endTime = duration_cast<milliseconds>(model->index(range.lower())
                    .data(Qn::TimestampRole).value<microseconds>());

                if (endTime <= 0ms)
                    period.durationMs = QnTimePeriod::kInfiniteDuration;
                else
                    period.setEndTime(endTime);
            }

            NX_ASSERT(period.isValid());
            if (!period.isValid())
                return;

            sinceLastRequest.restart();
            model->dynamicUpdate(period);
        }
    }
};

BookmarkSearchWidget::BookmarkSearchWidget(QnWorkbenchContext* context, QWidget* parent):
    base_type(context, new BookmarkSearchListModel(context), parent),
    d(new Private{qobject_cast<BookmarkSearchListModel*>(model())})
{
    NX_CRITICAL(d->model);

    setPlaceholderPixmap(qnSkin->pixmap("events/placeholders/bookmarks.png"));

    connect(this, &AbstractSearchWidget::textFilterChanged,
        [this](const QString& text)
        {
            d->model->setFilterText(text);
        });

    // Track remote bookmark changes by polling.
    QPointer<QTimer> updateTimer = new QTimer(this);
    updateTimer->setInterval(kUpdateTimerResolution);
    connect(updateTimer.data(), &QTimer::timeout, this, [this]() { d->updateVisiblePeriod(this); });

    installEventHandler(this, {QEvent::Show, QEvent::Hide}, this,
        [updateTimer](QObject* /*watched*/, QEvent* event)
        {
            if (event->type() == QEvent::Show)
                updateTimer->start();
            else
                updateTimer->stop();
        });

    connect(view(), &EventRibbon::visibleRangeChanged, this,
        [this]() { d->sinceViewUpdated.restart(); });

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged,
        this, &BookmarkSearchWidget::updateAllowance);

    connect(model(), &AbstractSearchListModel::isOnlineChanged,
        this, &BookmarkSearchWidget::updateAllowance);
}

BookmarkSearchWidget::~BookmarkSearchWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString BookmarkSearchWidget::placeholderText(bool constrained) const
{
    static const QString kHtmlPlaceholder =
        lit("<center><p>%1</p><p><font size='-3'>%2</font></p></center>")
        .arg(tr("No bookmarks"))
        .arg(tr("Select some period on timeline and click "
            "with right mouse button on it to create a bookmark."));

    return constrained ? tr("No bookmarks") : kHtmlPlaceholder;
}

QString BookmarkSearchWidget::itemCounterText(int count) const
{
    return tr("%n bookmarks", "", count);
}

bool BookmarkSearchWidget::calculateAllowance() const
{
    return model()->isOnline()
        && accessController()->hasGlobalPermission(vms::api::GlobalPermission::viewBookmarks);
}

} // namespace nx::vms::client::desktop
