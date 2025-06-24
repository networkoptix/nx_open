// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tab_api_backend.h"

#include <QtCore/QCoreApplication>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>

#include "helpers.h"
#include "resources_structures.h"

namespace nx::vms::client::desktop::jsapi::detail {

using namespace std::chrono;

namespace {

LayoutProperties propertiesState(QnWorkbenchLayout* layout)
{
    const auto layoutResource = layout->resource();
    if (!NX_ASSERT(layoutResource))
        return LayoutProperties();

    LayoutProperties result;
    const auto minimumSize = layoutResource->fixedSize();
    if (!minimumSize.isEmpty())
    {
        result.minimumSize = Size{minimumSize.width(), minimumSize.height()};
    }
    result.locked = layout->locked();
    return result;
}

QnTimePeriod periodFromTimeWindow(const TimePeriod& window)
{
    return QnTimePeriod{window.startTimeMs, window.durationMs};
}

TimePeriod TimeWindowFromPeriod(const QnTimePeriod& period)
{
    return TimePeriod{period.startTime(), period.duration()};
}

Error cantFindItemResult()
{
    return Error::invalidArguments(QCoreApplication::translate(
        "nx::vms::client::desktop::jsapi::detail::TabApiBackend",
        "Cannot find an item with the specified ID"));
}

Error cantFindWidgetResult()
{
    return Error::failed(TabApiBackend::tr(
        "Cannot find a widget corresponding to the specified item"));
}

} // namespace

struct TabApiBackend::Private: public QObject
{
    TabApiBackend* const q;
    WindowContext* const context;
    QPointer<QnWorkbenchLayout> layout;
    QPointer<QnWorkbenchDisplay> const display;
    QPointer<QnWorkbenchNavigator> const navigator;

    UuidSet selectedItems;
    nx::Uuid focusedItem;

    QSet<nx::Uuid> availableItems;
    nx::utils::ScopedConnections connections;

    Private(TabApiBackend* q, WindowContext* context, QnWorkbenchLayout* layout);
    ~Private();

    bool isCurrentLayout() const { return context->workbench()->currentLayout() == layout; }

    /** Connect to all workbench entities. Actual only when the layout is current. */
    void establishWorkbenchConnections();

    void updateLayoutItemData(
        const QUuid& itemId,
        const ItemParams& params);

    void updateItemParams(
        const QUuid& itemId,
        const ItemParams& params);

    void handleItemChanged(QnWorkbenchItem* item) const;

    Item itemState(QnWorkbenchItem* item) const;

    std::optional<MediaParams> itemMediaParams(QnWorkbenchItem* item) const;
    bool isFocusedWidget(const QnResourceWidget* widget) const;
    QnResourceWidget* focusedMediaWidget() const;

    bool isSyncedLayout() const;

    template<typename Ids>
    QList<Item> itemStatesFromIds(const Ids& ids) const;

    QList<Item> allItemStates() const;

    ItemResult itemOperationResult(
        const Error& result,
        const QUuid& itemId = QUuid()) const;

    void handleResourceWidgetAdded(QnResourceWidget* widget);

    bool hasPermissions(const QnResourcePtr& resource, Qn::Permissions permissions);

    void handleSliderChanged();
};

TabApiBackend::Private::Private(
    TabApiBackend* q,
    WindowContext* context,
    QnWorkbenchLayout* layout)
    :
    q(q),
    context(context),
    layout(layout),
    display(context->workbenchContext()->display()),
    navigator(context->workbenchContext()->navigator())
{
    if (!layout)
        return;

    NX_VERBOSE(this, "Integration tab object created for layout %1", layout->name());
    auto workbench = context->workbenchContext()->workbench();

    connect(workbench, &Workbench::currentLayoutChanged, this,
        [this]()
        {
            NX_VERBOSE(this, "Current layout changed");
            if (isCurrentLayout())
                establishWorkbenchConnections();
        });

    connect(workbench, &Workbench::currentLayoutAboutToBeChanged, this,
        [this]
        {
            connections.reset();
        });

    if (isCurrentLayout())
        establishWorkbenchConnections();

    const auto handleItemAdded =
        [this, q](QnWorkbenchItem* item)
        {
            connect(item, &QnWorkbenchItem::geometryChanged, q,
                [this, item]() { handleItemChanged(item); });
            connect(item, &QnWorkbenchItem::dataChanged, q,
                [this, item](int role)
                {
                    if (role == Qn::ItemSliderSelectionRole || role == Qn::ItemSliderWindowRole)
                        handleItemChanged(item);
                });
        };

    connect(layout, &QnWorkbenchLayout::itemAdded, this,
        [this, q, handleItemAdded](QnWorkbenchItem* addedItem)
        {
            NX_VERBOSE(this, "Layout item added handled");
            handleItemAdded(addedItem);
            emit q->itemAdded(itemState(addedItem));
        });

    for (const auto& item: layout->items())
        handleItemAdded(item);

    connect(layout, &QnWorkbenchLayout::itemRemoved, this,
        [this, q](QnWorkbenchItem* removedItem)
        {
            NX_VERBOSE(this, "Layout item removed handled");
            availableItems.remove(removedItem->uuid());
            emit q->itemRemoved(removedItem->uuid());
        });
}

TabApiBackend::Private::~Private()
{
    NX_VERBOSE(this, "Integration tab object is destroyed");
}

void TabApiBackend::Private::establishWorkbenchConnections()
{
    connections << connect(display, &QnWorkbenchDisplay::widgetAdded,
        this, &Private::handleResourceWidgetAdded);

    for (const auto& widget: display->widgets())
        handleResourceWidgetAdded(widget);

    const auto timeSlider = navigator->timeSlider();
    connections << connect(timeSlider, &QnTimeSlider::windowChanged,
        this, &Private::handleSliderChanged);
    connections << connect(timeSlider, &QnTimeSlider::selectionChanged,
        this, &Private::handleSliderChanged);
}

void TabApiBackend::Private::handleSliderChanged()
{
    if (!NX_ASSERT(layout))
        return;

    if (isSyncedLayout())
    {
        // Windows for all items of synced layout are changed simultaneously.
        for (const auto item: layout->items())
            handleItemChanged(item);
    }
    else if (const auto focused = focusedMediaWidget())
    {
        handleItemChanged(focused->item());
    }
}

void TabApiBackend::Private::updateLayoutItemData(
    const QUuid& itemId,
    const ItemParams& params)
{
    if (!NX_ASSERT(layout))
        return;

    auto layoutResource = layout->resource();

    // We always skip focus state for newly added items to preserve selection on layout.
    layoutResource->setItemData(itemId, Qn::ItemSkipFocusOnAdditionRole, true);

    if (!params.media)
        return;

    if (params.media->speed.has_value())
    {
        const qreal speed = params.media->speed.value();
        layoutResource->setItemData(itemId, Qn::ItemPausedRole, qFuzzyEquals(speed, 0));
        layoutResource->setItemData(itemId, Qn::ItemSpeedRole, speed);
    }

    if (params.media->timestampMs.has_value())
    {
        layoutResource->setItemData(itemId, Qn::ItemTimeRole,
            QVariant::fromValue<qint64>(params.media->timestampMs->count()));
    }

    if (params.media->timelineWindow.has_value())
    {
        layoutResource->setItemData(itemId, Qn::ItemSliderWindowRole,
            QVariant::fromValue(periodFromTimeWindow(params.media->timelineWindow.value())));
    }

    if (params.media->timelineSelection.has_value())
    {
        layoutResource->setItemData(itemId, Qn::ItemSliderSelectionRole,
            QVariant::fromValue(periodFromTimeWindow(params.media->timelineSelection.value())));
    }
}

void TabApiBackend::Private::updateItemParams(
    const QUuid& itemId,
    const ItemParams& params)
{
    if (!NX_ASSERT(layout))
        return;

    const auto item = layout->item(itemId);
    if (!item)
        return;

    const auto setParametersLater = nx::utils::guarded(item,
        [this, item, params]()
        {
            if (params.geometry.has_value())
            {
                const QRectF geometry(
                    params.geometry->pos.x, params.geometry->pos.y,
                    params.geometry->size.width, params.geometry->size.height);
                item->setCombinedGeometry(geometry);
            }

            if (params.selected.has_value())
                display->widget(item)->setSelected(params.selected.value());

            if (params.focused.has_value())
            {
                const auto workbench = context->workbench();
                if (params.focused.value())
                {
                    workbench->setItem(Qn::ActiveRole, item);
                    workbench->setItem(Qn::CentralRole, item);
                }
                else
                {
                    for (int role = Qn::RaisedRole; role != Qn::ItemRoleCount; ++role)
                    {
                        const Qn::ItemRole itemRole = static_cast<Qn::ItemRole>(role);
                        if (workbench->item(itemRole) == item)
                            workbench->setItem(itemRole, nullptr);
                    }
                }
            }

            if (!params.media)
                return;

            const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(display->widget(item));
            if (!mediaWidget)
                return;

            if (params.media->speed.has_value())
                mediaWidget->setSpeed(params.media->speed.value());

            if (params.media->timestampMs.has_value())
                mediaWidget->setPosition(params.media->timestampMs.value().count());

            if (!isFocusedWidget(mediaWidget))
                return; //< Do all timeline changes only if item is current one.

            const auto timeSlider = navigator->timeSlider();
            if (params.media->timelineWindow.has_value())
            {
                const bool stillPosition = timeSlider->options().testFlag(QnTimeSlider::StillPosition);
                if (stillPosition)
                    timeSlider->setOption(QnTimeSlider::StillPosition, false);

                const auto window = params.media->timelineWindow.value();
                const auto endTime = window.durationMs.count() != QnTimePeriod::kInfiniteDuration
                    ? window.startTimeMs + window.durationMs
                    : timeSlider->maximum();
                timeSlider->setWindow(window.startTimeMs, endTime, /*animate*/ false,
                    /*forceResize*/ true);
            }

            if (params.media->timelineSelection.has_value())
            {
                const auto selection = params.media->timelineSelection.value();
                timeSlider->setSelectionValid(selection.durationMs.count() > 0);
                if (selection.durationMs.count() > 0)
                    timeSlider->setSelection(selection.startTimeMs, selection.startTimeMs + selection.durationMs);
            }
        });

    // We update parameters later to give some time to the item to be added on the scene.
    executeDelayedParented(setParametersLater, milliseconds(1), this);
}

void TabApiBackend::Private::handleItemChanged(QnWorkbenchItem* item) const
{
    if (item && availableItems.contains(item->uuid()))
        emit q->itemChanged(itemState(item));
}

Item TabApiBackend::Private::itemState(QnWorkbenchItem* item) const
{
    if (!NX_ASSERT(layout) || !item)
        return {};

    const auto& itemId = item->uuid();

    Item result;
    result.id = itemId.toQUuid();
    result.resource = Resource::from(item->resource());

    result.params =
        [this, item, itemId, geometry = item->combinedGeometry()]()
        {
            ItemParams result;
            result.focused = itemId == focusedItem;
            result.selected = selectedItems.contains(itemId);
            result.geometry = Rect::from(geometry);
            result.media = itemMediaParams(item);
            return result;
        }();

    return result;
}

std::optional<MediaParams> TabApiBackend::Private::itemMediaParams(QnWorkbenchItem* item) const
{
    if (!NX_ASSERT(layout) || !item)
        return {};

    const auto type = resourceType(item->resource());
    if (!detail::hasMediaStream(type))
        return {};

    const auto widget = display->widget(item);
    const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
    if (!mediaWidget) //< This may be when we remove item from the scene.
        return {};

    MediaParams result;
    result.timestampMs = mediaWidget->position();
    result.speed = mediaWidget->speed();

    const bool synced = isSyncedLayout();

    const auto fillFromTimeSlider =
        [&result, this]()
        {
            const auto timeSlider = navigator->timeSlider();
            result.timelineSelection = TimeWindowFromPeriod(timeSlider->selection());

            const auto timeSliderMaximum = timeSlider->maximum();
            const auto timeSliderWindowEnd = timeSlider->windowEnd();
            const auto timeSliderWindowStart = timeSlider->windowStart();
            const qint64 duration = (timeSliderMaximum - timeSliderWindowEnd) < milliseconds(300)
                ? QnTimePeriod::kInfiniteDuration
                : (timeSliderWindowEnd - timeSliderWindowStart).count();

            result.timelineWindow = TimeWindowFromPeriod(
                QnTimePeriod(timeSliderWindowStart.count(), duration));
        };

    const auto tryFillFromItemData =
        [this, &result](const nx::Uuid& itemId)
        {
            auto layoutResource = layout->resource();
            if (!NX_ASSERT(layoutResource))
                return false;

            const auto selectionValue = layoutResource->itemData(itemId,
                Qn::ItemSliderSelectionRole);
            const auto windowValue = layoutResource->itemData(itemId,
                Qn::ItemSliderWindowRole);
            if (!selectionValue.isValid() || !windowValue.isValid())
                return false; //< We suppose these values should be set simultaneously.

            const auto selection = selectionValue.value<QnTimePeriod>();
            result.timelineSelection = TimeWindowFromPeriod(selection);

            const auto window = windowValue.value<QnTimePeriod>();
            result.timelineWindow = TimeWindowFromPeriod(window);
            return true;
        };

    const auto tryFillFromAnyLayoutItemData =
        [&]()
        {
            for (const auto anyLayoutItem: layout->items())
            {
                const auto type = resourceType(anyLayoutItem->resource());
                if (detail::hasMediaStream(type) && tryFillFromItemData(anyLayoutItem->uuid()))
                    break;
            }
        };

    if (isCurrentLayout())
    {
        const auto focused = focusedMediaWidget();

        if (synced)
        {
            // Slider window and selection are the same for the all items on a synced layout.
            if (focused)
            {
                // We have a focused media widget so time slider has correct
                // representation (not null) for all items on a synced layout.
                fillFromTimeSlider();
            }
            else
            {
                // If we don't have any media widget selected, as we have layout synced, we just
                // look for the stored item data for slider values.
                tryFillFromAnyLayoutItemData();
            }
        }
        else //< Not synced.
        {
            if (focused == mediaWidget)
            {
                // If current item is focused and layout is current then just update from the time
                // slider values.
                fillFromTimeSlider();
            }
            else
            {
                // If current item is not focused then just try to update from the item's data.
                tryFillFromItemData(item->uuid());
            }
        }
    }
    else //< Not a current layout
    {
        // For an invisible layout all we may to do is to try updating from the item's data.
        if (synced)
        {
            // On synced inactive layout any selection/window data from media item is acceptable.
            tryFillFromAnyLayoutItemData();
        }
        else
        {
            // No matter if item is focused or not, we just take all results from the item's data.
            tryFillFromItemData(item->uuid());
        }
    }

    return result;
}

QnResourceWidget* TabApiBackend::Private::focusedMediaWidget() const
{
    if (!isCurrentLayout())
        return nullptr;

    const auto widgets = context->workbenchContext()->display()->widgets();
    const auto it = std::find_if(widgets.cbegin(), widgets.cend(),
        [this](const QnResourceWidget* widget)
        {
            return isFocusedWidget(widget);
        });
    return it == widgets.cend() ? nullptr : *it;
}

bool TabApiBackend::Private::isFocusedWidget(const QnResourceWidget* widget) const
{
    if (!NX_ASSERT(layout) || !widget)
        return false;

    const auto state = widget->selectionState();
    switch (state)
    {
        case QnResourceWidget::SelectionState::focused:
        case QnResourceWidget::SelectionState::focusedAndSelected:
            return true;

        case QnResourceWidget::SelectionState::inactiveFocused:
        {
            // We suppose item is focused in the inactive state only if
            // there is no other active and focused item.
            const auto widgets = context->workbenchContext()->display()->widgets();
            return !std::any_of(widgets.cbegin(), widgets.cend(),
                [](const QnResourceWidget* widget)
                {
                    return widget->selectionState() == QnResourceWidget::SelectionState::focused;
                });
        }
        default:
            return false;
    };
}

bool TabApiBackend::Private::isSyncedLayout() const
{
    if (!NX_ASSERT(layout))
        return false;

    auto streamSynchronizer = context->workbench()->windowContext()->streamSynchronizer();

    const auto state = isCurrentLayout()
        ? streamSynchronizer->state()
        : layout->streamSynchronizationState();

    return state.isSyncOn;
}

template<typename Ids>
QList<Item> TabApiBackend::Private::itemStatesFromIds(const Ids& ids) const
{
    if (!NX_ASSERT(layout))
        return {};

    QList<Item> result;
    for (const auto& id: ids)
    {
        if (const auto item = layout->item(id))
            result.push_back(itemState(item));
    }
    return result;
}

QList<Item> TabApiBackend::Private::allItemStates() const
{
    if (!NX_ASSERT(layout))
        return {};

    UuidList itemIds;
    for (const auto& item: layout->items())
        itemIds.append(item->uuid());

    return itemStatesFromIds(itemIds);
}

ItemResult TabApiBackend::Private::itemOperationResult(
    const Error& error,
    const QUuid& itemId) const
{
    if (!NX_ASSERT(layout))
        return {};

    ItemResult itemResult;
    itemResult.error = error;

    if (!itemId.isNull())
        itemResult.item = itemState(layout->item(itemId));

    return itemResult;
}

void TabApiBackend::Private::handleResourceWidgetAdded(QnResourceWidget* widget)
{
    const auto item = widget->item();
    if (!item)
        return;

    if (!isCurrentLayout())
        return;

    const auto itemId = item->uuid();
    if (const auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget))
    {
        const auto processItemChanged = [this, item]() { handleItemChanged(item); };
        connect(mediaWidget, &QnMediaResourceWidget::positionChanged, this, processItemChanged);
        connect(mediaWidget, &QnMediaResourceWidget::speedChanged, this, processItemChanged);
    }

    const auto handleSelectedStateChanged =
        [this, item, itemId](QnResourceWidget* widget)
        {
            if (!NX_ASSERT(layout) || !widget || context->workbench()->isInLayoutChangeProcess())
                return;

            const auto state = widget->selectionState();
            const bool selected = state >= QnResourceWidget::SelectionState::selected;
            const bool focused = isFocusedWidget(widget);

            bool hasChanges = false;
            if (selectedItems.contains(itemId) != selected)
            {
                hasChanges = true;
                if (selected)
                    selectedItems.insert(itemId);
                else
                    selectedItems.remove(itemId);
            }

            if ((focusedItem == itemId) != focused)
            {
                hasChanges = true;
                focusedItem = focused
                    ? itemId
                    : nx::Uuid();
            }

            if (hasChanges)
                handleItemChanged(item);
        };

    connect(widget, &QnResourceWidget::selectionStateChanged, this,
        [handleSelectedStateChanged, widget]() { handleSelectedStateChanged(widget); });

    // Do not handle selection state change for widgets which were added after layout switch.
    // Since we destroy all widgets on layout switch, widget addition signal is not to be
    // processed as we already store correct state of selection for layout.
    if (availableItems.contains(itemId))
        return;

    availableItems.insert(itemId);
    handleSelectedStateChanged(widget);
}

bool TabApiBackend::Private::hasPermissions(
    const QnResourcePtr& resource, Qn::Permissions permissions)
{
    return ResourceAccessManager::hasPermissions(resource, permissions);
}

//-------------------------------------------------------------------------------------------------

TabApiBackend::TabApiBackend(
    WindowContext* context,
    QnWorkbenchLayout* layout,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, context, layout))
{
}

TabApiBackend::~TabApiBackend()
{
}

State TabApiBackend::state() const
{
    if (!NX_ASSERT(d->layout))
        return {};

    State result;
    result.items = d->allItemStates();
    result.sync = d->isSyncedLayout();
    result.selection = ItemSelection{
        d->itemState(d->layout->item(d->focusedItem)),
        d->itemStatesFromIds(d->selectedItems)
    };

    result.properties = propertiesState(d->layout);
    return result;
}

ItemResult TabApiBackend::item(const QUuid& itemId) const
{
    if (!NX_ASSERT(d->layout))
        return {Error::failed(), {}};

    const auto item = d->layout->item(itemId);
    return item
        ? ItemResult{Error::success(), d->itemState(item)}
        : ItemResult{cantFindItemResult(), {}};
}

ItemResult TabApiBackend::addItem(const ResourceUniqueId& resourceId, const ItemParams& params)
{
    if (!NX_ASSERT(d->layout))
        return {Error::failed(), {}};

    const auto resource = getResourceIfAvailable(resourceId);

    if (!resource)
    {
        return d->itemOperationResult(
            Error::invalidArguments(tr("Cannot find a resource with the specified ID.")));
    }

    if (params.media && !hasMediaStream(resourceType(resource)))
    {
        return d->itemOperationResult(Error::invalidArguments(
            tr("Cannot specify a media parameters for the resource without media stream.")));
    }

    if (!d->hasPermissions(resource, Qn::ViewContentPermission))
        return d->itemOperationResult(Error::denied());

    const auto actionParams = menu::Parameters(resource);
    if (!d->context->menu()->canTrigger(menu::OpenInCurrentLayoutAction, actionParams))
    {
        return d->itemOperationResult(
            Error::failed(tr("Cannot add the resource to the layout")));
    }

    common::LayoutItemData itemData = layoutItemFromResource(resource,
        /*forceCloud*/ d->layout->resource()->hasFlags(Qn::cross_system));
    itemData.flags = Qn::PendingGeometryAdjustment;

    const auto itemId = itemData.uuid.toQUuid();
    d->updateLayoutItemData(itemId, params);
    d->layout->resource()->addItem(itemData);
    d->updateItemParams(itemId, params);

    return d->itemOperationResult(Error::success(), itemId);
}

Error TabApiBackend::setItemParams(
    const QUuid& itemId,
    const ItemParams& params)
{
    if (!NX_ASSERT(d->layout))
        return Error::failed();

    const auto item = d->layout->item(itemId);
    if (!item)
        return cantFindItemResult();

    d->updateLayoutItemData(itemId, params);
    d->updateItemParams(itemId, params);
    return Error::success();
}

Error TabApiBackend::setMaximizedItem(const QString& itemId)
{
    if (!NX_ASSERT(d->layout))
        return Error::failed();

    const auto menu = d->context->menu();

    if (itemId.isEmpty())
    {
        menu->triggerForced(menu::UnmaximizeItemAction);
        return Error::success();
    }

    const auto item = d->layout->item(QUuid{itemId});
    if (!item)
        return cantFindItemResult();

    const auto widget = d->context->workbenchContext()->display()->widget(item);
    if (!widget)
        return cantFindWidgetResult();

    return menu->triggerIfPossible(menu::MaximizeItemAction, widget)
        ? Error::success()
        : Error::failed(tr("Cannot maximize the item"));
}

Error TabApiBackend::removeItem(const QUuid& itemId)
{
    if (!NX_ASSERT(d->layout))
        return Error::failed();

    const auto item = d->layout->item(itemId);
    if (!item)
        return cantFindItemResult();

    d->layout->removeItem(item);
    return Error::success();
}

Error TabApiBackend::syncWith(const QUuid& itemId)
{
    if (itemId.isNull())
        return Error::invalidArguments();

    if (!NX_ASSERT(d->layout))
        return Error::failed();

    const auto item = d->layout->item(itemId);
    if (!item)
        return cantFindItemResult();

    const auto widget = d->context->workbenchContext()->display()->widget(item);
    if (!widget)
        return cantFindWidgetResult();

    auto streamSynchronizer = d->context->workbench()->windowContext()->streamSynchronizer();
    streamSynchronizer->setState(widget, /*useWidgetPausedState*/true);
    return Error::success();
}

Error TabApiBackend::stopSyncPlay()
{
    if (!NX_ASSERT(d->layout))
        return Error::failed();

    if (d->isCurrentLayout())
    {
        auto streamSynchronizer = d->context->workbench()->windowContext()->streamSynchronizer();
        streamSynchronizer->setState(StreamSynchronizationState::disabled());
    }

    d->layout->setStreamSynchronizationState(StreamSynchronizationState::disabled());
    return Error::success();
}

Error TabApiBackend::setLayoutProperties(const LayoutProperties& properties)
{
    if (!NX_ASSERT(d->layout))
        return Error::failed();

    Qn::Permissions requiredPermissions;
    if (properties.locked.has_value())
        requiredPermissions |= Qn::EditLayoutSettingsPermission;

    if (properties.minimumSize.has_value())
        requiredPermissions |= Qn::SavePermission;

    const auto layoutResource = d->layout->resource();
    if (!d->hasPermissions(layoutResource, requiredPermissions))
        return Error::denied();

    if (auto locked = properties.locked)
        layoutResource->setLocked(*locked);

    if (auto size = properties.minimumSize)
        layoutResource->setFixedSize(QSize(size->width, size->height));

    return Error::success();
}

Error TabApiBackend::saveLayout()
{
    if (!NX_ASSERT(d->layout))
        return Error::failed();

    if (!d->hasPermissions(d->layout->resource(), Qn::SavePermission))
        return Error::denied();

    const auto layoutResource = d->layout->resource();
    const auto menu = d->context->menu();
    const bool success = menu->triggerIfPossible(menu::SaveLayoutAction, layoutResource);
    return success
        ? Error::success()
        : Error::failed();
}

QnWorkbenchLayout* TabApiBackend::layout() const
{
    return d->layout;
}

} // namespace nx::vms::client::desktop::jsapi::detail
