// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_list_model.h"

#include <QtWidgets/QWidget>

#include <client/client_globals.h>
#include <core/resource/resource.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <ui/common/notification_levels.h>
#include <utils/common/delayed.h>

#include "private/system_health_list_model_p.h"

namespace nx::vms::client::desktop {

namespace {

static const auto kDisplayTimeout = std::chrono::milliseconds(12500);
constexpr int kPaddings = 16;
constexpr int kMaxInvalidScheduleTooltipWidth = 320;

} // namespace

using MessageType = nx::vms::common::system_health::MessageType;

SystemHealthListModel::SystemHealthListModel(WindowContext* context, QObject* parent):
    base_type(context, parent),
    d(new Private(this))
{
}

SystemHealthListModel::~SystemHealthListModel()
{
}

int SystemHealthListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->count();
}

QVariant SystemHealthListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            return d->text(index.row());

        case Qn::TimestampRole:
            return d->timestamp(index.row());

        case Qn::DecorationPathRole:
            return d->decorationPath(index.row());

        case Qn::ResourceListRole:
            return QVariant::fromValue(d->displayedResourceList(index.row()));

        case Qt::ToolTipRole:
            return d->toolTip(index.row());

        case Qt::ForegroundRole:
        {
            const auto color = d->color(index.row());
            return color.isValid() ? QVariant::fromValue(color) : QVariant();
        }

        case Qn::CommandActionRole:
            return QVariant::fromValue(d->commandAction(index.row()));

        case Qn::AlternateColorRole:
            return true;

        case Qn::RemovableRole:
            return d->isCloseable(index.row());

        case Qn::PriorityRole:
            return d->priority(index.row());

        case Qn::NotificationLevelRole:
            return QVariant::fromValue(QnNotificationLevel::valueOf(d->message(index.row())));

        case Qn::TimeoutRole:
            return d->locked(index.row())
                ? QVariant()
                : QVariant::fromValue(kDisplayTimeout);

        case Qn::MaximumTooltipSizeRole:
            if (d->messageType(index.row()) == MessageType::cameraRecordingScheduleIsInvalid)
            {
                return QSize(
                    kMaxInvalidScheduleTooltipWidth, mainWindowWidget()->height() - kPaddings);
            }
            break;

        case Qn::PreviewTimeRole:
            if (d->messageType(index.row()) == MessageType::cameraRecordingScheduleIsInvalid)
                return QVariant(); //< No preview

            break;

        default:
            break;
    }
    return base_type::data(index, role);
}

bool SystemHealthListModel::defaultAction(const QModelIndex& index)
{
    const auto action = d->action(index.row());
    if (action == menu::NoAction)
        return false;

    menu()->triggerIfPossible(action, d->parameters(index.row()));

    if (d->message(index.row()) == MessageType::cloudPromo)
        showOnceSettings()->cloudPromo = true;

    return true;
}

bool SystemHealthListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || count < 0 || row + count > rowCount())
        return false;

    for (int i = 0; i < count; ++i)
    {
        if (d->message(row + i) == MessageType::cloudPromo)
        {
            executeLater([this]() { showOnceSettings()->cloudPromo = true; }, this);
            break;
        }
    }

    d->remove(row, count);
    return true;
}

} // namespace nx::vms::client::desktop
