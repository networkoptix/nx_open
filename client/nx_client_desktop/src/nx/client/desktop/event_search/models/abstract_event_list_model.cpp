#include "abstract_event_list_model.h"

#include <chrono>

#include <translation/datetime_formatter.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/synctime.h>

#include <nx/utils/datetime.h>

namespace nx::client::desktop {

AbstractEventListModel::AbstractEventListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(context)
{
    NX_CRITICAL(context, "Workbench context must be specified.");
}

QVariant AbstractEventListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    switch (role)
    {
        case Qn::TimestampTextRole:
            return timestampText(index.data(Qn::TimestampRole).value<qint64>());

        case Qt::AccessibleTextRole:
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            return index.data(Qt::DisplayRole);

        case Qt::AccessibleDescriptionRole:
            return index.data(Qn::DescriptionTextRole);

        case Qn::AnimatedRole:
            return true;

        default:
            return QVariant();
    }
}

bool AbstractEventListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    switch (role)
    {
        case Qn::DefaultNotificationRole:
            return defaultAction(index);

        case Qn::ActivateLinkRole:
            return activateLink(index, value.toString());

        default:
            return base_type::setData(index, value, role);
    }
}

bool AbstractEventListModel::isValid(const QModelIndex& index) const
{
    return index.model() == this && !index.parent().isValid() && index.column() == 0
        && index.row() >= 0 && index.row() < rowCount();
}

QString AbstractEventListModel::timestampText(qint64 timestampUs) const
{
    if (timestampUs <= 0)
        return QString();

    using namespace std::chrono;
    const auto timestampMs = duration_cast<milliseconds>(microseconds(timestampUs)).count();
    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs);
    if (qnSyncTime->currentDateTime().date() != dateTime.date())
        return datetime::toString(dateTime.date());
    else
        return datetime::toString(dateTime.time());
}

bool AbstractEventListModel::defaultAction(const QModelIndex& index)
{
    if (!isValid(index) || index.model() != this)
        return false;

    // TODO: #vkutin Introduce a new QnAction instead of direct access.
    auto slider = navigator()->timeSlider();
    if (!slider)
        return false;

    const auto timestampUsVariant = index.data(Qn::TimestampRole);
    if (!timestampUsVariant.canConvert<qint64>())
        return false;

    const QnScopedTypedPropertyRollback<bool, QnTimeSlider> downRollback(slider,
        &QnTimeSlider::setSliderDown,
        &QnTimeSlider::isSliderDown,
        true);

    using namespace std::chrono;
    const auto timestampMs = duration_cast<milliseconds>(microseconds(
        timestampUsVariant.value<qint64>()));

    slider->setValue(timestampMs, true);
    return true;
}

bool AbstractEventListModel::activateLink(const QModelIndex& index, const QString& /*link*/)
{
    return defaultAction(index);
}

} // namespace nx::client::desktop
