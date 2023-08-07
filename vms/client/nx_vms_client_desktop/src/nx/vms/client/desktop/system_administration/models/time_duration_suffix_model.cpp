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

    switch ((Suffix) m_suffixes.at(index.row()))
    {
        case Suffix::seconds:
            return QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Seconds, m_value);
        case Suffix::minutes:
            return QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Minutes, m_value);
        case Suffix::hours:
            return QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Hours, m_value);
        case Suffix::days:
            return QnTimeStrings::fullSuffix(QnTimeStrings::Suffix::Days, m_value);

        default:
            break;
    };

    return {};
}

int TimeDurationSuffixModel::multiplierAt(int row)
{
    if (row < 0 || row >= m_suffixes.size())
        return -1;

    switch ((Suffix) m_suffixes.at(row))
    {
        case Suffix::minutes:
            return 60;

        case Suffix::hours:
            return 3600;

        case Suffix::days:
            return 3600 * 24;

        case Suffix::seconds:
            return 1;

        default:
            return -1;
    }
}

int TimeDurationSuffixModel::rowCount(const QModelIndex&) const
{
    return m_suffixes.size();
}

void TimeDurationSuffixModel::setValue(int value)
{
    if (value == m_value)
        return;

    m_value = value;
    emit valueChanged();

    emit dataChanged(index(0), index(rowCount() - 1), {Qt::DisplayRole});
}

QList<int> TimeDurationSuffixModel::suffixes() const
{
    return m_suffixes;
}

void TimeDurationSuffixModel::setSuffixes(const QList<int> &suffixes)
{
    if (m_suffixes == suffixes)
        return;

    const int diff = suffixes.size() - m_suffixes.size();
    if (diff > 0)
    {
        beginInsertRows({}, m_suffixes.size(), m_suffixes.size() + diff - 1);
        m_suffixes = suffixes;
        endInsertRows();
    }
    else if (diff < 0)
    {
        beginRemoveRows({}, m_suffixes.size() + diff, m_suffixes.size() - 1);
        m_suffixes = suffixes;
        endRemoveRows();
    }
    else
    {
        m_suffixes = suffixes;
    }

    if (m_suffixes.size() > 0)
    {
        emit dataChanged(
            this->index(0, 0),
            this->index(m_suffixes.size() - 1, 0));
    }

    emit suffixesChanged();
}

} // namespace nx::vms::client::desktop
