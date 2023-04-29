// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_duration_suffix_model.h"

#include <nx/vms/text/time_strings.h>

namespace nx::vms::client::desktop {

TimeDurationSuffixModel::TimeDurationSuffixModel()
{
}

TimeDurationSuffixModel::~TimeDurationSuffixModel()
{
}

void TimeDurationSuffixModel::registerQmlType()
{
    qmlRegisterType<TimeDurationSuffixModel>(
        "nx.vms.client.desktop", 1, 0, "TimeDurationSuffixModel");
}

QVariant TimeDurationSuffixModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= rowCount() || role != Qt::DisplayRole)
        return {};

    switch ((Suffix) index.row())
    {
        case Suffix::secondsIndex:
            return QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Seconds, m_value);
        case Suffix::minutesIndex:
            return QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Minutes, m_value);
        case Suffix::hoursIndex:
            return QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Hours, m_value);

        default:
            break;
    };

    return {};
}

int TimeDurationSuffixModel::multiplierAt(int row)
{
    switch ((Suffix) row)
    {
        case Suffix::minutesIndex:
            return 60;

        case Suffix::hoursIndex:
            return 3600;

        case Suffix::secondsIndex:
        default:
            return 1;
    }
}

int TimeDurationSuffixModel::rowCount(const QModelIndex&) const
{
    return (int) Suffix::count;
}

void TimeDurationSuffixModel::setValue(int value)
{
    if (value == m_value)
        return;

    m_value = value;
    emit valueChanged();

    emit dataChanged(index(0), index(rowCount() - 1), {Qt::DisplayRole});
}

} // namespace nx::vms::client::desktop
