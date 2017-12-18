#include "found_devices_model.h"

#include <core/resource_management/resource_pool.h>

namespace {

using IdsSet = QSet<QString>;
IdsSet extractIds(const QnManualResourceSearchList& devices)
{
    IdsSet result;
    for (const auto& device: devices)
        result.insert(device.uniqueId);

    return result;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

FoundDevicesModel::FoundDevicesModel(QObject* parent):
    base_type(parent)
{
}

void FoundDevicesModel::addDevices(const QnManualResourceSearchList& devices)
{
    auto newIds = extractIds(devices);
    const auto currentIds = extractIds(m_devices);
    const auto truncatedIds = newIds.subtract(currentIds);
    if (truncatedIds.size() != devices.size())
        NX_EXPECT(false, "Devices exist already");

    const int first = rowCount();
    const int last = rowCount() + truncatedIds.size() - 1;
    const ScopedInsertRows guard(this, QModelIndex(), first, last);
    bool hasNewDeivces = false;
    for (const auto& newDevice: devices)
    {
        if (truncatedIds.contains(newDevice.uniqueId))
        {
            hasNewDeivces = true;
            m_devices.append(newDevice);
            m_checked.insert(newDevice.uniqueId, false);
            m_presentedState.insert(newDevice.uniqueId,
                newDevice.existsInPool ? alreadyAddedState : notPresentedState);

        }
    }

    // Update numbers in header
    if (hasNewDeivces)
    {
        headerDataChanged(Qt::Horizontal,
            FoundDevicesModel::presentedStateColumn, FoundDevicesModel::presentedStateColumn);
        emit rowCountChanged();
    }
}

void FoundDevicesModel::removeDevices(QStringList ids)
{
    bool someDevicesRemoved = false;
    while(!ids.isEmpty())
    {
        const auto id = ids.back();
        ids.removeLast();

        const auto it = std::find_if(m_devices.begin(), m_devices.end(),
            [id](const QnManualResourceSearchEntry& entry)
            {
                return entry.uniqueId == id;
            });

        if (it == m_devices.end())
        {
            NX_EXPECT(false, "Item does not exist");
            continue;
        }

        const int index = it - m_devices.begin();
        const ScopedRemoveRows guard(this, QModelIndex(), index, index);
        m_checked.remove(id);
        m_presentedState.remove(id);
        m_devices.erase(it);
        someDevicesRemoved = true;
    }

    // Update numbers in header
    if (someDevicesRemoved)
    {
        headerDataChanged(Qt::Horizontal,
            FoundDevicesModel::presentedStateColumn, FoundDevicesModel::presentedStateColumn);
        emit rowCountChanged();
    }
}

QModelIndex FoundDevicesModel::indexByUniqueId(const QString& uniqueId, int column)
{
    const auto it = std::find_if(m_devices.begin(), m_devices.end(),
        [uniqueId](const QnManualResourceSearchEntry& entry)
        {
            return entry.uniqueId == uniqueId;
        });

    return it == m_devices.end()
        ? QModelIndex()
        : index(it - m_devices.begin(), column);
}

int FoundDevicesModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_devices.size();

    NX_EXPECT(false, "Wrong parent index");
    return 0;
}

bool FoundDevicesModel::correctIndex(const QModelIndex& index) const
{
    const bool correct = index.isValid() && index.row() >= 0 && index.row() < rowCount();
    if (!correct)
        NX_EXPECT(false, "Invalid index");

    return correct;
}

QVariant FoundDevicesModel::displayData(const QModelIndex& index) const
{
    if (!correctIndex(index))
        return QVariant();

    const auto& currentDeivce = device(index);
    switch(index.column())
    {
        case Columns::nameColumn:
            return currentDeivce.name;
        case Columns::urlColumn:
            return currentDeivce.url;
        case Columns::manufacturerColumn:
            return currentDeivce.vendor;
        default:
            return QVariant();
    }
}

QnManualResourceSearchEntry FoundDevicesModel::device(const QModelIndex& index) const
{
    return correctIndex(index)
        ? m_devices[index.row()]
        : QnManualResourceSearchEntry();
}

QVariant FoundDevicesModel::checkedData(const QModelIndex& index) const
{
    if (!correctIndex(index) || index.column() != FoundDevicesModel::checkboxColumn)
        return QVariant();

    return m_checked[device(index).uniqueId]
        ? Qt::Checked
        : Qt::Unchecked;
}

QVariant FoundDevicesModel::data(const QModelIndex& index, int role) const
{
    if (!correctIndex(index))
        return QVariant();

    switch(role)
    {
        case Qt::DisplayRole:
            return displayData(index);
        case Qt::CheckStateRole:
            return checkedData(index);
        case dataRole:
            return QVariant::fromValue(device(index));
        case presentedStateRole:
            return m_presentedState[device(index).uniqueId];
        default:
            return QVariant();
    }
}

bool FoundDevicesModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (!index.isValid())
        return false;

    if (index.column() == checkboxColumn && role == Qt::CheckStateRole)
    {
        const auto uniqueId = m_devices[index.row()].uniqueId;
        m_checked[uniqueId] = value.value<Qt::CheckState>() == Qt::Unchecked ? false : true;
        emit dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }
    else if (index.column() == presentedStateColumn && role == presentedStateRole)
    {

        const auto uniqueId = device(index).uniqueId;
        m_presentedState[uniqueId] = value.value<PresentedState>();
        emit dataChanged(index, index, {presentedStateRole});
        return true;
    }

    return base_type::setData(index, value, role);
}

QVariant FoundDevicesModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch(section)
    {
        case Columns::manufacturerColumn:
            return tr("Brand");
        case Columns::nameColumn:
            return tr("Model");
        case Columns::urlColumn:
            return tr("Address");
        case Columns::presentedStateColumn:
            return tr("%1 devices total, %2 new ").arg(rowCount()).arg(newDevicesCount());
        default:
            return QVariant();
    }
}

int FoundDevicesModel::columnCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return Columns::count;

    NX_EXPECT(false, "Wrong parent index");
    return 0;
}

int FoundDevicesModel::newDevicesCount() const
{
    const auto addedDevicesCount = std::count_if(m_devices.begin(), m_devices.end(),
        [](const QnManualResourceSearchEntry& entry) { return entry.existsInPool; });

    return rowCount() - addedDevicesCount;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

