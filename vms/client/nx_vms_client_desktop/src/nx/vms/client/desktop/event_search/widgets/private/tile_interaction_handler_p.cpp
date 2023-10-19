// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tile_interaction_handler_p.h"

#include <QtCore/QModelIndex>
#include <QtGui/QClipboard>
#include <QtGui/QDrag>
#include <QtGui/QPainter>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollArea>

#include <chrono>

#include <analytics/db/analytics_db_types.h>
#include <api/model/analytics_actions.h>
#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/action_type_descriptor_manager.h>
#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/metatypes.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/analytics/analytics_actions_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/bookmarks/bookmark_utils.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/right_panel/models/right_panel_models_adapter.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/text/human_readable.h>
#include <nx/vms/time/formatter.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/utils/table_export_helper.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/camera/camera_names_watcher.h>
#include <utils/common/delayed.h>

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
    return doInstall(QnWorkbenchContextAware(ribbon).context(), ribbon);
}

TileInteractionHandler* TileInteractionHandler::install(RightPanelModelsAdapter* adapter)
{
    return doInstall(adapter->context(), adapter);
}

template<typename T>
TileInteractionHandler* TileInteractionHandler::doInstall(
    QnWorkbenchContext* context, T* tileInteractionSource)
{
    if (!NX_ASSERT(context && tileInteractionSource))
        return nullptr;

    const auto previous = tileInteractionSource->template findChild<TileInteractionHandler*>();
    if (previous)
        return previous;

    const auto handler = new TileInteractionHandler(context, tileInteractionSource);

    connect(tileInteractionSource, &T::linkActivated, handler,
        [](const QModelIndex& index, const QString& link)
        {
            if (!NX_ASSERT(index.isValid()))
                return;

            const auto model = const_cast<QAbstractItemModel*>(index.model());
            model->setData(index, link, Qn::ActivateLinkRole);
        });

    connect(tileInteractionSource, &T::clicked,
        handler, &TileInteractionHandler::handleClick);

    connect(tileInteractionSource, &T::doubleClicked,
        handler, &TileInteractionHandler::handleDoubleClick);

    connect(tileInteractionSource, &T::dragStarted,
        handler, &TileInteractionHandler::performDragAndDrop);

    connect(tileInteractionSource, &T::contextMenuRequested,
        handler, &TileInteractionHandler::showContextMenu);

    connect(tileInteractionSource, &T::pluginActionRequested,
        handler, &TileInteractionHandler::executePluginAction);

    connect(handler, &TileInteractionHandler::highlightedResourcesChanged,
        tileInteractionSource, &T::setHighlightedResources);

    connect(handler, &TileInteractionHandler::highlightedTimestampChanged,
        tileInteractionSource, &T::setHighlightedTimestamp);

    return handler;
}

TileInteractionHandler::TileInteractionHandler(QnWorkbenchContext* context, QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    m_showPendingMessages(new nx::utils::PendingOperation())
{
    if (!NX_ASSERT(context))
        return;

    m_showPendingMessages->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_showPendingMessages->setCallback(
        [this]()
        {
            for (const auto& message: m_pendingMessages)
                showMessage(message);

            m_pendingMessages.clear();
        });

    const auto updateHighlightedResources =
        [this]()
        {
            emit highlightedResourcesChanged(workbench()->currentLayout()
                ? nx::utils::toQSet(workbench()->currentLayout()->itemResources())
                : QSet<QnResourcePtr>());
        };

    connect(workbench(), &Workbench::currentLayoutChanged, this, updateHighlightedResources);
    connect(workbench(), &Workbench::currentLayoutItemsChanged, this, updateHighlightedResources);

    connect(navigator(), &QnWorkbenchNavigator::timelinePositionChanged, this,
        [this]() { emit highlightedTimestampChanged(microseconds(navigator()->positionUsec())); });
}

void TileInteractionHandler::handleClick(
    const QModelIndex& index, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
    if (!NX_ASSERT(index.isValid()))
        return;

    const auto actionSupport = checkActionSupport(index);

    switch (actionSupport)
    {
        case ActionSupport::supported:
            break;

        case ActionSupport::unsupported:
            return;

        case ActionSupport::interactionRequired:
        {
            const auto cloudSystemId = index.data(Qn::CloudSystemIdRole).toString();
            if (auto context = appContext()->cloudCrossSystemManager()->systemContext(cloudSystemId))
            {
                if (context->initializeConnectionWithUserInteraction())
                {
                    auto model = const_cast<QAbstractItemModel*>(index.model());
                    model->setData(index, true, Qn::ForcePreviewLoaderRole);
                }
            }

            // Display connection UI along with banner.
            [[fallthrough]];
        }

        case ActionSupport::crossSystem:
            showMessage(tr("This action is not supported for notifications from other Systems"));
            return;

        default:
            NX_ASSERT(false, "Unexpected action support: %1", (int)actionSupport);
            return;
    }

    if (button == Qt::LeftButton && !modifiers.testFlag(Qt::ControlModifier))
    {
        const auto model = const_cast<QAbstractItemModel*>(index.model());
        if (!model->setData(index, QVariant(), Qn::DefaultNotificationRole))
            navigateToSource(index, /*instantMessages*/ false);
    }
    else if ((button == Qt::LeftButton && modifiers.testFlag(Qt::ControlModifier))
        || button == Qt::MiddleButton)
    {
        openSource(index, /*inNewTab*/ true, /*fromDoubleClick*/ false);
    }
}

void TileInteractionHandler::handleDoubleClick(const QModelIndex& index)
{
    if (checkActionSupport(index) == ActionSupport::supported)
        openSource(index, /*inNewTab*/ false, /*fromDoubleClick*/ true);
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
        .filtered(&QnResourceAccessFilter::isOpenableInLayout);

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
        menu()->trigger(GoToLayoutItemAction, resource);

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

void TileInteractionHandler::executePluginAction(
    const QnUuid& engineId,
    const QString& actionTypeId,
    const nx::analytics::db::ObjectTrack& track,
    const QnVirtualCameraResourcePtr& camera) const
{
    if (!connection())
        return;

    const nx::analytics::ActionTypeDescriptorManager descriptorManager(systemContext());
    const auto actionDescriptor = descriptorManager.descriptor(actionTypeId);
    if (!actionDescriptor)
        return;

    AnalyticsAction actionData;
    actionData.engineId = engineId;
    actionData.actionId = actionDescriptor->id;
    actionData.objectTrackId = track.id;
    actionData.timestampUs = track.firstAppearanceTimeUs;
    actionData.deviceId = camera->getId();

    if (!actionDescriptor->parametersModel.isEmpty())
    {
        // Show dialog to enter required parameters.
        const auto params = AnalyticsActionsHelper::requestSettingsMap(
            actionDescriptor->parametersModel, mainWindowWidget());

        if (!params)
            return;

        actionData.params = *params;
    }

    const auto resultCallback =
        [this, camera, engineId](
            bool success,
            rest::Handle /*requestId*/,
            nx::network::rest::JsonResult result)
        {
            if (result.error != nx::network::rest::Result::NoError)
            {
                QnMessageBox::warning(mainWindowWidget(), tr("Failed to execute plugin action"),
                    result.errorString);
                return;
            }

            if (!success)
                return;

            const auto reply = result.deserialized<AnalyticsActionResult>();
            AnalyticsActionsHelper::processResult(
                reply,
                workbench()->context(),
                resourcePool()->getResourceById(engineId),
                /*authenticator*/ {},
                mainWindowWidget());
        };

    connectedServerApi()->executeAnalyticsAction(
        actionData, nx::utils::guarded(this, resultCallback), thread());
}

void TileInteractionHandler::copyBookmarkToClipboard(const QModelIndex &index)
{
    auto displayTime =
        [](qint64 msecsSinceEpoch)
        {
            // TODO: #sivanov Actualize used system context.
            const auto timeWatcher = appContext()->currentSystemContext()->serverTimeWatcher();
            return nx::vms::time::toString(timeWatcher->displayTime(msecsSinceEpoch));
        };

    enum class HeaderItem
    {
        name, camera, startTime, length, created, creator, tags, description
    };

    const QList<QPair<HeaderItem, QString>> headers{
        {HeaderItem::name, tr("Name")},
        {HeaderItem::camera, tr("Camera")},
        {HeaderItem::startTime, tr("Start time")},
        {HeaderItem::length, tr("Length")},
        {HeaderItem::created, tr("Created")},
        {HeaderItem::creator, tr("Creator")},
        {HeaderItem::tags, tr("Tags")},
        {HeaderItem::description, tr("Description")}};

    if (index.isValid())
    {
        using namespace nx::vms::common;

        QString textHeader;
        QString textData;
        QString htmlData;
        {
            html::Tag htmlTag("html", htmlData, html::Tag::BothBreaks);
            html::Tag bodyTag("body", htmlData, html::Tag::BothBreaks);
            html::Tag tableTag("table", htmlData, html::Tag::BothBreaks);

            { /* Creating header. */
                html::Tag tableRowTag("tr", htmlData);
                for (const auto& header: headers)
                {
                    html::Tag tableHeaderTag("th", htmlData);
                    htmlData.append(header.second);

                    if (!textHeader.isEmpty())
                        textHeader.append('\t');
                    textHeader.append(header.second);
                }
            }

            { /* Fill table with data */
                QString cellValue;
                auto bookmark = index.data(Qn::CameraBookmarkRole).value<QnCameraBookmark>();
                html::Tag tableRowTag("tr", htmlData);
                for (const auto& header: headers)
                {
                    html::Tag tableDataTag("td", htmlData);
                    switch (header.first)
                    {
                        case HeaderItem::name:
                            cellValue = bookmark.name;
                            break;

                        case HeaderItem::camera:
                        {
                            cellValue = systemContext()->cameraNamesWatcher()->getCameraName(
                                bookmark.cameraId);
                            break;
                        }

                        case HeaderItem::startTime:
                            cellValue = displayTime(bookmark.startTimeMs.count());
                            break;

                        case HeaderItem::length:
                        {
                            const auto duration = std::chrono::milliseconds(
                                std::abs(bookmark.durationMs.count()));
                            using HumanReadable = nx::vms::text::HumanReadable;
                            cellValue = HumanReadable::timeSpan(duration,
                                HumanReadable::DaysAndTime,
                                HumanReadable::SuffixFormat::Full,
                                ", ",
                                HumanReadable::kNoSuppressSecondUnit);
                            break;
                        }

                        case HeaderItem::created:
                            cellValue = displayTime(bookmark.creationTime().count());
                            break;

                        case HeaderItem::creator:
                            cellValue =
                                getBookmarkCreatorName(bookmark.creatorId, systemContext());
                            break;

                        case HeaderItem::tags:
                            cellValue = QnCameraBookmark::tagsToString(bookmark.tags);
                            break;

                        case HeaderItem::description:
                            cellValue = bookmark.description;
                            break;
                    };

                    htmlData.append(cellValue.toHtmlEscaped());

                    if (!textData.isEmpty())
                        textData.append('\t');
                    textData.append(cellValue);
                }
            }
        }

        QMimeData* mimeData = new QMimeData();
        mimeData->setText(textHeader + '\n' + textData);
        mimeData->setHtml(htmlData);
        QApplication::clipboard()->setMimeData(mimeData);
    }
}

TileInteractionHandler::ActionSupport TileInteractionHandler::checkActionSupport(
    const QModelIndex& index)
{
    const auto cloudSystemId = index.data(Qn::CloudSystemIdRole).toString();
    if (cloudSystemId.isEmpty())
        return ActionSupport::supported;

    const auto actionType = index.data(Qn::ActionIdRole).value<ui::action::IDType>();

    if (actionType == ui::action::BrowseUrlAction)
        return ActionSupport::unsupported;

    if (actionType != ui::action::NoAction)
    {
        const auto previewResource = index.data(Qn::ResourceRole).value<QnResourcePtr>();

        // Show interaction dialog for tiles with preview and special action,
        // different from adding a camera to the layout.
        if (previewResource && previewResource->hasFlags(Qn::ResourceFlag::fake))
            return ActionSupport::interactionRequired;

        return ActionSupport::crossSystem;
    }

    return ActionSupport::supported;
}

void TileInteractionHandler::openSource(
    const QModelIndex& index, bool inNewTab, bool fromDoubleClick)
{
    NX_ASSERT(checkActionSupport(index) == ActionSupport::supported);

    auto resourceList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered(&QnResourceAccessFilter::isOpenableInLayout);

    const auto currentLayout = workbench()->currentLayout();
    if (fromDoubleClick && currentLayout && NX_ASSERT(!inNewTab))
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

    const auto playbackStarter = scopedPlaybackStarter(!inNewTab
        && ini().startPlaybackOnTileNavigation);

    Parameters parameters(resourceList);
    const auto arguments = setupDropActionParameters(resourceList, index.data(Qn::TimestampRole));

    for (const auto& param: nx::utils::constKeyValueRange(arguments))
        parameters.setArgument(param.first, param.second);

    const auto action = inNewTab ? OpenInNewTabAction : DropResourcesAction;
    menu()->trigger(action, parameters);
}

void TileInteractionHandler::performDragAndDrop(
    const QModelIndex& index, const QPoint& pos, const QSize& size)
{
    const auto resourceList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered(&QnResourceAccessFilter::isDroppable);

    if (resourceList.empty())
        return;

    QScopedPointer<QMimeData> baseMimeData(index.model()->mimeData({index}));
    if (!baseMimeData)
        return;

    MimeData data(baseMimeData.get());
    data.setResources(resourceList);
    data.setArguments(setupDropActionParameters(resourceList, index.data(Qn::TimestampRole)));

    QScopedPointer<QDrag> drag(new QDrag(this));
    drag->setMimeData(data.createMimeData());
    drag->setPixmap(createDragPixmap(resourceList, size.width()));
    drag->setHotSpot({pos.x(), 0});

    drag->exec(Qt::CopyAction);
}

void TileInteractionHandler::showContextMenu(
    const QModelIndex& index,
    const QPoint& globalPos,
    bool withStandardInteraction,
    QWidget* parent)
{
    if (!NX_ASSERT(index.isValid()))
        return;

    auto menu = index.data(Qn::CreateContextMenuRole).value<QSharedPointer<QMenu>>();

    const auto addMenuActions =
        [&menu](QList<QAction*> actions, bool atTheBeginning = false)
        {
            if (!menu || !NX_ASSERT(!menu->isEmpty()))
                menu.reset(new QMenu(), &QObject::deleteLater);

            if (menu->isEmpty())
            {
                menu->addActions(actions);
            }
            else if (atTheBeginning)
            {
                menu->insertSeparator(menu->actions()[0]);
                menu->insertActions(menu->actions()[0], actions);
            }
            else
            {
                menu->addSeparator();
                menu->addActions(actions);
            }
        };

    if (withStandardInteraction)
    {
        const auto resourceList = index.data(Qn::ResourceListRole).value<QnResourceList>()
            .filtered(&QnResourceAccessFilter::isOpenableInLayout);

        Qn::Permissions requiredPermissions = Qn::ViewLivePermission;
        const auto timestamp = index.data(Qn::TimestampRole);
        if (timestamp.isValid())
        {
            const auto timeUs = timestamp.value<microseconds>().count();
            if (timeUs >= 0 && timeUs != DATETIME_NOW)
                requiredPermissions = Qn::ViewFootagePermission;
        }
        if (!resourceList.empty() && std::any_of(resourceList.cbegin(), resourceList.cend(),
            [requiredPermissions, accessController = systemContext()->accessController()]
            (const auto& resource)
            {
                return accessController->hasPermissions(resource, requiredPermissions);
            }))
        {
            const auto openAction = new QAction(tr("Open"), menu.get());
            connect(openAction, &QAction::triggered, this,
                [this, index = QPersistentModelIndex(index)]()
                {
                    if (index.isValid())
                        openSource(index, /*inNewTab*/ false, /*fromDoubleClick*/ false);
                });

            const auto openInNewTabAction = new QAction(tr("Open in New Tab"), menu.get());
            connect(openInNewTabAction, &QAction::triggered, this,
                [this, index = QPersistentModelIndex(index)]()
                {
                    if (index.isValid())
                        openSource(index, /*inNewTab*/ true, /*fromDoubleClick*/ false);
                });

            addMenuActions({openAction, openInNewTabAction}, /*atTheBeginning*/ true);
        }
    }

    const auto bookmark = index.data(Qn::CameraBookmarkRole);
    if (bookmark.isValid())
    {
        const auto exportBookmarkAction =
            new QAction(action(CopyBookmarkTextAction)->text(), menu.get());

        connect(exportBookmarkAction, &QAction::triggered, this,
            [this, index]()
            {
                copyBookmarkToClipboard(index);
            });

        addMenuActions({exportBookmarkAction});
    }

    if (!menu)
        return;

    // Avoid capturing menu shared pointer in lambda because it creates a reference cycle.
    const auto model = const_cast<QAbstractItemModel*>(index.model());
    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, menu.get(),
        [menu = menu.get(), index = QPersistentModelIndex(index)](
            const QModelIndex& parent, int first, int last)
        {
            if (!parent.isValid() && index.row() >= first && index.row() <= last)
                menu->close();
        });

    menu->setParent(parent, menu->windowFlags());
    menu->exec(globalPos);
}

QPixmap TileInteractionHandler::createDragPixmap(const QnResourceList& resources, int width) const
{
    if (resources.empty())
        return {};

    static constexpr int kMaximumRows = 10;
    static constexpr int kFontPixelSize = 13;
    static constexpr auto kFontWeight = QFont::Medium;
    static constexpr int kTextIndent = 4;

    static constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;

    const auto iconSize = qnSkin->maximumSize(qnResIconCache->icon(QnResourceIconCache::Camera));

    const int overflow = qMax(resources.size() - kMaximumRows, 0);
    const int resourceRows = resources.size() - overflow;
    const int totalRows = overflow ? (resourceRows + 1) : resourceRows;

    const int height = totalRows * iconSize.height();

    const auto devicePixelRatio = qApp->devicePixelRatio();
    const auto palette = qApp->palette();

    QFont font;
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
    const auto id = SceneBanners::instance()->add(text);
    m_messages.insert(id);

    connect(SceneBanners::instance(), &SceneBanners::removed, this,
        [this](const QnUuid& id) { m_messages.remove(id); });
}

void TileInteractionHandler::showMessageDelayed(const QString& text, milliseconds delay)
{
    m_pendingMessages.push_back(text);
    m_showPendingMessages->setInterval(delay); //< Restart timer.
    m_showPendingMessages->requestOperation();
}

void TileInteractionHandler::hideMessages()
{
    const auto messages = m_messages;
    for (const auto& id: messages)
        SceneBanners::instance()->remove(id);

    m_messages.clear();
    m_pendingMessages.clear();
}

nx::utils::Guard TileInteractionHandler::scopedPlaybackStarter(bool baseCondition)
{
    if (!baseCondition)
        return {};

    return nx::utils::Guard(
        [this, oldPosition = navigator()->positionUsec()]()
        {
            if (oldPosition == navigator()->positionUsec())
                return;

            navigator()->setSpeed(1.0);
            navigator()->setPlaying(true);
        });
}

} // namespace nx::vms::client::desktop
