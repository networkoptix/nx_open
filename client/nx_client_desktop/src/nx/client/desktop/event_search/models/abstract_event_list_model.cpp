#include "abstract_event_list_model.h"

#include <QtCore/QDateTime>

#include <utils/common/synctime.h>
#include <utils/common/delayed.h>

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

        default:
            return QVariant();
    }
}

bool AbstractEventListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return false;
}

void AbstractEventListModel::fetchMore(const QModelIndex& /*parent*/)
{
}

bool AbstractEventListModel::prefetchAsync(PrefetchCompletionHandler /*completionHandler*/)
{
    return false;
}

void AbstractEventListModel::commitPrefetch(qint64 /*keyLimitFromSync*/)
{
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

QString AbstractEventListModel::debugTimestampToString(qint64 timestampMs)
{
    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(Qt::RFC2822Date);
}

bool AbstractEventListModel::fetchInProgress() const
{
    return false; //< By default fetch is considered synchronous.
}

} // namespace
} // namespace client
} // namespace nx
