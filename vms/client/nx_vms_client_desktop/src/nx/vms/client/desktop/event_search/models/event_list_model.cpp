// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_list_model.h"

#include <QtCore/QDateTime>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/event_search/utils/event_search_utils.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/workbench/workbench_context.h>

#include "private/event_list_model_p.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;

nx::Uuid EventListModel::EventData::sourceId() const
{
    return source ? source->getId() : nx::Uuid();
}

EventListModel::EventListModel(WindowContext* context, QObject* parent):
    base_type(parent),
    WindowContextAware(context),
    d(new Private(this))
{
    setSystemContext(context->system());
}

EventListModel::~EventListModel()
{
}

int EventListModel::rowCount(const QModelIndex& /*parent*/) const
{
    return d->count();
}

QVariant EventListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return {};

    const auto& event = d->getEvent(index.row());
    switch (role)
    {
        case Qt::DisplayRole:
            return QVariant::fromValue(event.title);

        case core::DecorationPathRole:
            return event.iconPath;

        case core::UuidRole:
            return QVariant::fromValue(event.id);

        case core::TimestampRole:
            return QVariant::fromValue(event.timestamp);

        case core::PreviewTimeRole:
            return QVariant::fromValue(event.previewTime);

        case core::ForcePrecisePreviewRole:
            return event.previewTime > 0us && event.previewTime.count() != DATETIME_NOW;

        case Qt::ToolTipRole:
            return QVariant::fromValue(event.toolTip);

        case core::DescriptionTextRole:
            return QVariant::fromValue(event.description);

        case Qn::NotificationLevelRole:
            return QVariant::fromValue(event.level);

        case Qt::ForegroundRole:
            return event.titleColor.isValid()
                ? QVariant::fromValue(event.titleColor)
                : QVariant();

        case core::ResourceRole:
            return QVariant::fromValue<QnResourcePtr>(d->previewCamera(event));

        case core::ResourceListRole:
            return QVariant::fromValue<QnResourceList>(d->accessibleCameras(event));

        case core::DisplayedResourceListRole:
            if (event.source)
                return QVariant::fromValue<QnResourceList>({event.source});
            else if (!event.sourceName.isEmpty())
                return QVariant::fromValue<QStringList>({event.sourceName});
            else
                return QVariant();

        case Qn::RemovableRole:
            return event.removable;

        case Qn::CommandActionRole:
            return QVariant::fromValue(event.extraAction);

        case Qn::OnCloseActionRole:
            return QVariant::fromValue(event.onCloseAction);

        case Qn::TimeoutRole:
            return event.removable ? QVariant::fromValue(event.lifetime) : QVariant();

        case core::ObjectTrackIdRole:
            return QVariant::fromValue(event.objectTrackId);

        case core::AnalyticsAttributesRole:
            return QVariant::fromValue(event.attributes);

        case Qn::CloudSystemIdRole:
            return QVariant::fromValue(event.cloudSystemId);

        case Qn::ActionIdRole:
            return QVariant::fromValue(event.actionId);

        case Qn::ForcePreviewLoaderRole:
            return event.forcePreviewLoader;

        case Qn::ShowVideoPreviewRole:
            return false;

        default:
            return base_type::data(index, role);
    }
}

bool EventListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!isValid(index))
        return false;

    if (role == Qn::ForcePreviewLoaderRole)
    {
        auto event = d->getEvent(index.row());
        event.forcePreviewLoader = value.toBool();
        d->updateEvent(event);

        return true;
    }

    return base_type::setData(index, value, role);
}

bool EventListModel::addEvent(const EventData& event, Position where)
{
    return where == Position::front
        ? d->addFront(event)
        : d->addBack(event);
}

std::list<EventListModel::EventData> EventListModel::addEvents(
    std::list<EventData> events, Position where)
{
    return where == Position::front
        ? d->addFront(std::move(events))
        : d->addBack(std::move(events));
}

bool EventListModel::updateEvent(const EventData& event)
{
    return d->updateEvent(event);
}

bool EventListModel::updateEvent(nx::Uuid id)
{
    return d->updateEvent(id);
}

QModelIndex EventListModel::indexOf(const nx::Uuid& id) const
{
    return index(d->indexOf(id));
}

EventListModel::EventData EventListModel::getEvent(int row) const
{
    return d->getEvent(row);
}

void EventListModel::clear()
{
    d->clear();
}

bool EventListModel::removeEvent(const nx::Uuid& id)
{
    return d->removeEvent(id);
}

bool EventListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || count < 0 || row + count > rowCount())
        return false;

    d->removeEvents(row, count);
    return true;
}

bool EventListModel::defaultAction(const QModelIndex& index)
{
    const auto& event = getEvent(index.row());
    if (event.actionId == menu::NoAction)
        return false;

    menu()->triggerIfPossible(event.actionId, event.actionParameters);
    return true;
}

} // namespace nx::vms::client::desktop
