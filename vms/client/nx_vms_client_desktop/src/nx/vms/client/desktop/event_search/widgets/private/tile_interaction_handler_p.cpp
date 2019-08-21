#include "tile_interaction_handler_p.h"

#include <QtCore/QModelIndex>
#include <QtGui/QDrag>
#include <QtWidgets/QApplication>

#include <client/client_globals.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace ui::action;

namespace {

milliseconds doubleClickInterval()
{
    return milliseconds(QApplication::doubleClickInterval());
}

bool isMediaResource(const QnResourcePtr& resource)
{
    return resource->hasFlags(Qn::media);
}

} // namespace

TileInteractionHandler::~TileInteractionHandler()
{
}

TileInteractionHandler* TileInteractionHandler::install(EventRibbon* ribbon)
{
    NX_ASSERT(ribbon);
    if (!ribbon)
        return nullptr;

    auto previous = ribbon->findChild<TileInteractionHandler*>();
    return previous ? previous : new TileInteractionHandler(ribbon);
}

TileInteractionHandler::TileInteractionHandler(EventRibbon* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_ribbon(parent),
    m_showPendingMessages(new nx::utils::PendingOperation()),
    m_navigateDelayed(new nx::utils::PendingOperation())
{
    NX_ASSERT(m_ribbon);
    if (!m_ribbon)
        return;

    m_showPendingMessages->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_showPendingMessages->setCallback(
        [this]()
        {
            for (const auto& message: m_pendingMessages)
                showMessage(message);

            m_pendingMessages.clear();
        });

    m_navigateDelayed->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);

    connect(m_ribbon.data(), &EventRibbon::linkActivated, this,
        [this](const QModelIndex& index, const QString& link)
        {
            m_ribbon->model()->setData(index, link, Qn::ActivateLinkRole);
        });

    connect(m_ribbon.data(), &EventRibbon::clicked, this,
        [this](const QModelIndex& index, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
        {
            if (button == Qt::LeftButton && !modifiers.testFlag(Qt::ControlModifier))
            {
                if (!m_ribbon->model()->setData(index, QVariant(), Qn::DefaultNotificationRole))
                {
                    if (isSyncOn())
                    {
                        navigateToSource(index, /*instantMessages*/ false);
                    }
                    else
                    {
                        m_navigateDelayed->setInterval(doubleClickInterval()); //< Restart timer.
                        m_navigateDelayed->setCallback(
                            [this, index = QPersistentModelIndex(index)]()
                            {
                                navigateToSource(index, /*instantMessages*/ true);
                            });

                        m_navigateDelayed->requestOperation();
                    }
                }
            }
            else if ((button == Qt::LeftButton && modifiers.testFlag(Qt::ControlModifier))
                || button == Qt::MiddleButton)
            {
                openSource(index, /*inNewTab*/ true, /*fromDoubleClick*/ false);
            }
        });

    connect(m_ribbon.data(), &EventRibbon::doubleClicked, this,
        [this](const QModelIndex& index)
        {
            openSource(index, /*inNewTab*/ false, /*fromDoubleClick*/ true);
        });

    connect(m_ribbon.data(), &EventRibbon::openRequested, this,
        [this](const QModelIndex& index, bool inNewTab)
        {
            openSource(index, inNewTab, /*fromDoubleClick*/ false);
        });

    connect(m_ribbon.data(), &EventRibbon::dragStarted,
        this, &TileInteractionHandler::performDragAndDrop);

    const auto updateHighlightedResources = nx::utils::guarded(m_ribbon,
        [this]()
        {
            m_ribbon->setHighlightedResources(workbench()->currentLayout()
                ? workbench()->currentLayout()->itemResources().toSet()
                : QSet<QnResourcePtr>());
        });

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this, updateHighlightedResources);
    connect(workbench(), &QnWorkbench::currentLayoutItemsChanged, this, updateHighlightedResources);

    connect(navigator(), &QnWorkbenchNavigator::timelinePositionChanged, this,
        nx::utils::guarded(m_ribbon,
            [this]()
            {
                m_ribbon->setHighlightedTimestamp(
                    microseconds(navigator()->positionUsec()));
            }));
}

void TileInteractionHandler::navigateToSource(
    const QPersistentModelIndex& index, bool instantMessages)
{
    // Obtain requested time.
    const auto timestamp = index.data(Qn::TimestampRole);
    if (!timestamp.canConvert<microseconds>())
        return;

    // Obtain requested camera list.
    const auto resourceList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered(&isMediaResource);

    const auto showMessage =
        [this](const QString& message, bool instant)
        {
            if (instant)
                this->showMessage(message);
            else
                showMessageDelayed(message, doubleClickInterval());
        };

    QnResourceList openResources;

    // Find which cameras are opened on current layout.
    if (auto layout = workbench()->currentLayout())
    {
        openResources = resourceList.filtered(
            [layout](const QnResourcePtr& resource)
            {
                return !layout->items(resource).empty();
            });
    }

    // If not all cameras are opened, show proper message after double click delay.
    if (openResources.size() != resourceList.size())
    {
        const auto message = tr("Double click to add cameras to the current layout "
            "or ctrl+click to open in a new tab", "", resourceList.size());

        showMessage(message, instantMessages);
    }

    // Single-select first of opened requested cameras.
    const auto resource = openResources.empty() ? QnResourcePtr() : openResources.front();
    if (resource)
    {
        menu()->trigger(GoToLayoutItemAction, Parameters(resource)
            .withArgument(Qn::RaiseSelectionRole, ini().raiseCameraFromClickedTile));
    }

    // If no relevant resource is open on current layout, do no navigation.
    if (openResources.empty())
        return;

    // If requested time is outside timeline range, show proper message after double click delay.
    const auto timelineRange = navigator()->timelineRange();
    auto navigationTime = timestamp.value<microseconds>();

    if (!navigator()->isTimelineRelevant()
        || !timelineRange.contains(duration_cast<milliseconds>(navigationTime)))
    {
        showMessage(tr("No available archive"), instantMessages);
        return;
    }

    const auto playbackStarter = scopedPlaybackStarter(navigationTime != microseconds(DATETIME_NOW)
        && ini().startPlaybackOnTileNavigation);

    // Perform navigation.
    menu()->triggerIfPossible(JumpToTimeAction,
        Parameters().withArgument(Qn::TimestampRole, navigationTime));
}

QHash<int, QVariant> TileInteractionHandler::setupDropActionParameters(
    const QnResourceList& resources, const QVariant& timestamp) const
{
    QHash<int, QVariant> parameters;
    parameters[Qn::SelectOnOpeningRole] = true;

    parameters[Qn::ItemTimeRole] = QVariant::fromValue<qint64>(timestamp.canConvert<microseconds>()
        ? duration_cast<milliseconds>(timestamp.value<microseconds>()).count()
        : milliseconds(DATETIME_NOW).count());

    if (resources.size() == 1 && navigator()->currentResource() == resources[0])
    {
        parameters[Qn::ItemSliderWindowRole] = QVariant::fromValue(navigator()->timelineWindow());

        parameters[Qn::ItemSliderSelectionRole] =
            QVariant::fromValue(navigator()->timelineSelection());

        if (auto widget = navigator()->currentMediaWidget())
        {
            parameters[Qn::ItemMotionSelectionRole] = QVariant::fromValue(widget->motionSelection());
            parameters[Qn::ItemAnalyticsSelectionRole] = widget->analyticsFilterRect();
        }
    }

    return parameters;
}

void TileInteractionHandler::openSource(
    const QModelIndex& index, bool inNewTab, bool fromDoubleClick)
{
    auto resourceList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered(&isMediaResource);

    const auto currentLayout = workbench()->currentLayout();
    if (fromDoubleClick && currentLayout && NX_ASSERT(!inNewTab) && isSyncOn())
    {
        resourceList = resourceList.filtered(
            [currentLayout](const QnResourcePtr& resource)
            {
                // Add only resources not opened on the current layout.
                return currentLayout->items(resource).empty();
            });
    }

    if (resourceList.empty())
        return;

    hideMessages();
    m_navigateDelayed->setCallback({});

    const auto playbackStarter = scopedPlaybackStarter(!inNewTab
        && ini().startPlaybackOnTileNavigation);

    Parameters parameters(resourceList);
    const auto arguments = setupDropActionParameters(resourceList, index.data(Qn::TimestampRole));

    for (const auto& param: nx::utils::keyValueRange(arguments))
        parameters.setArgument(param.first, param.second);

    const auto action = inNewTab ? OpenInNewTabAction : DropResourcesAction;
    menu()->trigger(action, parameters);
}

void TileInteractionHandler::performDragAndDrop(
    const QModelIndex& index, const QPoint& pos, const QSize& size)
{
    const auto resourceList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered(&isMediaResource);

    if (resourceList.empty())
        return;

    QScopedPointer<QMimeData> baseMimeData(index.model()->mimeData({index}));
    if (!baseMimeData)
        return;

    MimeData data(baseMimeData.get(), nullptr);
    data.setResources(resourceList);
    data.setArguments(setupDropActionParameters(resourceList, index.data(Qn::TimestampRole)));

    QScopedPointer<QDrag> drag(new QDrag(this));
    drag->setMimeData(data.createMimeData());
    drag->setPixmap(createDragPixmap(resourceList, size.width()));
    drag->setHotSpot({pos.x(), 0});

    drag->exec(Qt::CopyAction);
}

QPixmap TileInteractionHandler::createDragPixmap(const QnResourceList& resources, int width) const
{
    if (resources.empty())
        return {};

    static constexpr int kMaximumRows = 10;
    static constexpr int kFontPixelSize = 13;
    static constexpr int kFontWeight = QFont::Medium;
    static constexpr int kTextIndent = 4;

    static constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;

    const auto iconSize = qnSkin->maximumSize(qnResIconCache->icon(QnResourceIconCache::Camera));

    const int overflow = qMax(resources.size() - kMaximumRows, 0);
    const int resourceRows = resources.size() - overflow;
    const int totalRows = overflow ? (resourceRows + 1) : resourceRows;

    const int height = totalRows * iconSize.height();

    const auto devicePixelRatio = qApp->devicePixelRatio();
    const auto palette = m_ribbon->palette();

    auto font = m_ribbon->font();
    font.setPixelSize(kFontPixelSize);
    font.setWeight(kFontWeight);

    QPixmap target(QSize(width, height) * devicePixelRatio);
    target.setDevicePixelRatio(devicePixelRatio);
    target.fill(palette.color(QPalette::Window));

    QPainter painter(&target);
    painter.setPen(palette.color(QPalette::Text));
    painter.setFont(font);

    QColor background = palette.color(QPalette::Highlight);
    background.setAlphaF(style::Hints::kDisabledItemOpacity);
    painter.fillRect(QRect(0, 0, width, height), background);

    QRect rect(style::Metrics::kStandardPadding, 0, width, iconSize.height());
    for (int i = 0; i < resourceRows; ++i)
    {
        const auto icon = qnResIconCache->icon(resources[i]);
        icon.paint(&painter, rect, {Qt::AlignLeft | Qt::AlignVCenter}, QIcon::Selected);

        const auto text = QnResourceDisplayInfo(resources[i]).name();
        const auto textRect = rect.adjusted(iconSize.width() + kTextIndent, 0, 0, 0);
        painter.drawText(textRect,
            kTextFlags,
            QFontMetrics(font).elidedText(text, Qt::ElideRight, textRect.width()));

        rect.moveTop(rect.top() + iconSize.height());
    }

    if (overflow)
    {
        font.setWeight(QFont::Normal);
        painter.setFont(font);
        painter.setPen(palette.color(QPalette::WindowText));

        painter.drawText(rect.adjusted(kTextIndent, 0, 0, 0),
            kTextFlags,
            tr("... and %n more", "", overflow));
    }

    return target;
}

void TileInteractionHandler::showMessage(const QString& text)
{
    const auto box = QnGraphicsMessageBox::information(text);
    m_messages.insert(box);

    connect(box, &QObject::destroyed, this,
        [this](QObject* sender) { m_messages.remove(static_cast<QnGraphicsMessageBox*>(sender)); });
}

void TileInteractionHandler::showMessageDelayed(const QString& text, milliseconds delay)
{
    m_pendingMessages.push_back(text);
    m_showPendingMessages->setInterval(delay); //< Restart timer.
    m_showPendingMessages->requestOperation();
}

void TileInteractionHandler::hideMessages()
{
    for (auto box: m_messages)
        box->hideAnimated();

    m_pendingMessages.clear();
}

utils::Guard TileInteractionHandler::scopedPlaybackStarter(bool baseCondition)
{
    if (!baseCondition)
        return {};

    return utils::Guard(
        [this, oldPosition = navigator()->positionUsec()]()
        {
            if (oldPosition == navigator()->positionUsec())
                return;

            navigator()->setSpeed(1.0);
            navigator()->setPlaying(true);
        });
}

bool TileInteractionHandler::isSyncOn() const
{
    return navigator()->syncIsForced() || action(ui::action::ToggleSyncAction)->isChecked();
}

} // namespace nx::vms::client::desktop
