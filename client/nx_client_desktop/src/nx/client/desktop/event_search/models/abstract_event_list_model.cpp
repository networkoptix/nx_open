#include "abstract_event_list_model.h"

#include <QtCore/QDateTime>

#include <ui/graphics/items/controls/time_slider.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/synctime.h>
#include <utils/common/delayed.h>

#include <nx/utils/datetime.h>

namespace nx {
namespace client {
namespace desktop {

AbstractEventListModel::AbstractEventListModel(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
}

AbstractEventListModel::~AbstractEventListModel()
{
}

QVariant AbstractEventListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    switch (role)
    {
        case Qn::TimestampTextRole:
            return timestampText(index.data(Qn::TimestampRole).value<qint64>());

        case Qn::ResourceSearchStringRole:
            return lit("%1 %2")
                .arg(index.data(Qt::DisplayRole).toString())
                .arg(index.data(Qn::DescriptionTextRole).toString());

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

bool AbstractEventListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return false;
}

void AbstractEventListModel::fetchMore(const QModelIndex& /*parent*/)
{
}

bool AbstractEventListModel::fetchInProgress() const
{
    return false;
}

bool AbstractEventListModel::isConstrained() const
{
    return m_relevantTimePeriod != QnTimePeriod::anytime();
}

bool AbstractEventListModel::isValid(const QModelIndex& index) const
{
    return index.model() == this && !index.parent().isValid() && index.column() == 0
        && index.row() >= 0 && index.row() < rowCount();
}

QString AbstractEventListModel::timestampText(qint64 timestampMs) const
{
    if (timestampMs <= 0)
        return QString();

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs);
    if (qnSyncTime->currentDateTime().date() != dateTime.date())
        return dateTime.date().toString(Qt::DefaultLocaleShortDate);

    return dateTime.time().toString(Qt::DefaultLocaleShortDate);
}

bool AbstractEventListModel::defaultAction(const QModelIndex& index)
{
    if (!isValid(index) || index.model() != this)
        return false;

    // TODO: #vkutin Introduce a new QnAction instead of direct access.
    auto slider = navigator()->timeSlider();
    if (!slider)
        return false;

    const auto timestampMsVariant = index.data(Qn::TimestampRole);
    if (!timestampMsVariant.canConvert<qint64>())
        return false;

    const QnScopedTypedPropertyRollback<bool, QnTimeSlider> downRollback(slider,
        &QnTimeSlider::setSliderDown,
        &QnTimeSlider::isSliderDown,
        true);

    slider->setValue(timestampMsVariant.value<qint64>(), true);
    return true;
}

bool AbstractEventListModel::activateLink(const QModelIndex& index, const QString& /*link*/)
{
    return defaultAction(index);
}

const QnTimePeriod& AbstractEventListModel::relevantTimePeriod() const
{
    return m_relevantTimePeriod;
}

void AbstractEventListModel::setRelevantTimePeriod(const QnTimePeriod& value)
{
    if (value == m_relevantTimePeriod)
        return;

    const auto previousValue = m_relevantTimePeriod;
    m_relevantTimePeriod = value;

    relevantTimePeriodChanged(previousValue);
}

AbstractEventListModel::FetchDirection AbstractEventListModel::fetchDirection() const
{
    return m_fetchDirection;
}

void AbstractEventListModel::setFetchDirection(FetchDirection value)
{
    if (m_fetchDirection == value)
        return;

    m_fetchDirection = value;
    fetchDirectionChanged();
}

int AbstractEventListModel::maximumCount() const
{
    return m_maximumCount;
}

void AbstractEventListModel::setMaximumCount(int value)
{
    if (m_maximumCount == value)
        return;

    m_maximumCount = value;
    maximumCountChanged();
}

void AbstractEventListModel::maximumCountChanged()
{
}

int AbstractEventListModel::fetchBatchSize() const
{
    return m_fetchBatchSize;
}

void AbstractEventListModel::setFetchBatchSize(int value)
{
    m_fetchBatchSize = value;
}

QnTimePeriod AbstractEventListModel::fetchedTimeWindow() const
{
    return QnTimePeriod();
}

void AbstractEventListModel::relevantTimePeriodChanged(const QnTimePeriod& /*previousValue*/)
{
}

void AbstractEventListModel::fetchDirectionChanged()
{
}

void AbstractEventListModel::beginFinishFetch()
{
    emit fetchAboutToBeFinished(QPrivateSignal());
}

void AbstractEventListModel::endFinishFetch(bool cancelled)
{
    emit fetchFinished(cancelled, QPrivateSignal());
}

} // namespace desktop
} // namespace client
} // namespace nx
