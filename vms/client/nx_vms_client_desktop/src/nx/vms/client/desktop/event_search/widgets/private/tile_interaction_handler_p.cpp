#include "tile_interaction_handler_p.h"

#include <QtCore/QModelIndex>
#include <QtGui/QDrag>
#include <QtWidgets/QApplication>

#include <client/client_globals.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/pending_operation.h>
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
    m_showPendingMessages(new nx::utils::PendingOperation())
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
                    navigateToSource(index);
            }
            else if ((button == Qt::LeftButton && modifiers.testFlag(Qt::ControlModifier))
                || button == Qt::MiddleButton)
            {
                openSource(index, true /*inNewTab*/);
            }
        });

    connect(m_ribbon.data(), &EventRibbon::doubleClicked, this,
        [this](const QModelIndex& index)
        {
            openSource(index, false /*inNewTab*/);
        });

    connect(m_ribbon.data(), &EventRibbon::dragStarted,
        this, &TileInteractionHandler::performDragAndDrop);
}

void TileInteractionHandler::navigateToSource(const QModelIndex& index)
{
    // Obtain requested time.
    const auto timestamp = index.data(Qn::TimestampRole);
    if (!timestamp.canConvert<microseconds>())
        return;

    // Obtain requested camera list.
    const auto cameraList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>();

    QnVirtualCameraResourceList openCameras;

    // Find which cameras are opened on current layout.
    if (auto layout = workbench()->currentLayout())
    {
        openCameras = cameraList.filtered(
            [layout](const QnVirtualCameraResourcePtr& camera)
            {
                return !layout->items(camera).empty();
            });
    }

    // If not all cameras are opened, show proper message after double click delay.
    if (openCameras.size() != cameraList.size())
    {
        const auto message = tr("Double click to add camera(s) to current layout "
            "or ctrl+click to open in a new tab.", "", cameraList.size());

        showMessageDelayed(message, doubleClickInterval());
    }

    // Single-select first of opened requested cameras.
    const auto camera = openCameras.empty() ? QnVirtualCameraResourcePtr() : openCameras.front();
    if (camera)
    {
        menu()->trigger(GoToLayoutItemAction, Parameters(camera)
            .withArgument(Qn::RaiseSelectionRole, ini().raiseCameraFromClickedTile));
    }

    // If timeline is hidden, do no navigation.
    if (!navigator()->isTimelineRelevant())
        return;

    // If requested time is outside timeline range, show proper message after double click delay.
    const auto timelineRange = navigator()->timelineRange();
    auto navigationTime = timestamp.value<microseconds>();

    if (!timelineRange.contains(duration_cast<milliseconds>(navigationTime)))
    {
        showMessageDelayed(tr("No available archive."), doubleClickInterval());
        return;
    }

    // In case of requested time within the last minute, navigate to live instead.
    if (const bool lastMinute = navigationTime > (timelineRange.endTime() - 1min))
        navigationTime = microseconds(DATETIME_NOW);

    // Perform navigation.
    menu()->triggerIfPossible(JumpToTimeAction,
        Parameters().withArgument(Qn::TimestampRole, navigationTime));
}

void TileInteractionHandler::openSource(const QModelIndex& index, bool inNewTab)
{
    const auto cameraList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>();

    if (cameraList.empty())
        return;

    Parameters parameters(cameraList);
    parameters.setArgument(Qn::SelectOnOpeningRole, true);

    const auto timestamp = index.data(Qn::TimestampRole);
    if (timestamp.canConvert<microseconds>())
    {
        parameters.setArgument(Qn::ItemTimeRole,
            duration_cast<milliseconds>(timestamp.value<microseconds>()).count());
    }

    hideMessages();

    const auto action = inNewTab ? OpenInNewTabAction : DropResourcesAction;
    menu()->trigger(action, parameters);
}

void TileInteractionHandler::performDragAndDrop(
    const QModelIndex& index, const QPoint& pos, const QSize& size)
{
    const auto cameraList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>();

    if (cameraList.empty())
        return;

    QScopedPointer<QMimeData> baseMimeData(index.model()->mimeData({index}));
    if (!baseMimeData)
        return;

    QHash<int, QVariant> arguments;
    arguments[Qn::SelectOnOpeningRole] = true;

    const auto timestamp = index.data(Qn::TimestampRole);
    arguments[Qn::ItemTimeRole] = timestamp.canConvert<microseconds>()
        ? duration_cast<milliseconds>(timestamp.value<microseconds>()).count()
        : milliseconds(DATETIME_NOW).count();

    MimeData data(baseMimeData.get(), nullptr);
    data.setResources(cameraList);
    data.setArguments(arguments);

    QScopedPointer<QDrag> drag(new QDrag(this));
    drag->setMimeData(data.createMimeData());
    drag->setPixmap(createDragPixmap(cameraList, size.width()));
    drag->setHotSpot({pos.x(), 0});

    drag->exec(Qt::CopyAction);
}

QPixmap TileInteractionHandler::createDragPixmap(
    const QnVirtualCameraResourceList& cameras, int width) const
{
    if (cameras.empty())
        return {};

    static constexpr int kMaximumRows = 10;
    static constexpr int kFontPixelSize = 13;
    static constexpr int kFontWeight = QFont::Medium;
    static constexpr int kTextIndent = 4;

    static constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;

    const auto iconSize = qnSkin->maximumSize(qnResIconCache->icon(QnResourceIconCache::Camera));

    const int overflow = qMax(cameras.size() - kMaximumRows, 0);
    const int cameraRows = cameras.size() - overflow;
    const int totalRows = overflow ? (cameraRows + 1) : cameraRows;

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
    for (int i = 0; i < cameraRows; ++i)
    {
        const auto icon = qnResIconCache->icon(cameras[i]);
        icon.paint(&painter, rect, {Qt::AlignLeft | Qt::AlignVCenter}, QIcon::Selected);

        const auto text = QnResourceDisplayInfo(cameras[i]).name();
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
    m_showPendingMessages->setInterval(delay);
    m_showPendingMessages->requestOperation();
}

void TileInteractionHandler::hideMessages()
{
    for (auto box: m_messages)
        box->hideAnimated();

    m_pendingMessages.clear();
}

} // namespace nx::vms::client::desktop
