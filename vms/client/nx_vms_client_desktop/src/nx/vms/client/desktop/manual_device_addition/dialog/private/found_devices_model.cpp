#include "found_devices_model.h"

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

using IdsSet = QSet<QString>;
IdsSet extractIds(const QnManualResourceSearchList& devices)
{
    IdsSet result;
    for (const auto& device: devices)
        result.insert(device.uniqueId);

    return result;
}

QFont selectionColumnFont()
{
    QFont result;
    result.setPixelSize(13);
    result.setWeight(40);
    return result;
}

} // namespace

namespace nx::vms::client::desktop {

FoundDevicesModel::FoundDevicesModel(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
}

void FoundDevicesModel::addDevices(const QnManualResourceSearchList& devices)
{
    const auto pool = commonModule()->resourcePool();

    auto newIds = extractIds(devices);
    const auto currentIds = extractIds(m_devices);
    const auto addedIds = newIds.subtract(currentIds);
    if (addedIds.size() != devices.size())
        NX_ASSERT(false, "Devices exist already");

    if (addedIds.isEmpty())
        return; //< Nothing new has found.

    const int first = rowCount();
    const int last = rowCount() + addedIds.size() - 1;
    {
        const ScopedInsertRows guard(this, QModelIndex(), first, last);
        for (const auto& device: devices)
        {
            const auto& id = device.uniqueId;
            if (addedIds.contains(id))
            {
                const PresentedState presentedState =
                    device.existsInPool ? alreadyAddedState : notPresentedState;
                const bool isChecked = false;

                m_devices.append(device);
                m_deviceRowState.insert(id, {presentedState, isChecked});
                incrementDeviceCount({presentedState, isChecked});
            }
        }
    }
}

void FoundDevicesModel::removeDevices(QStringList ids)
{
    while (!ids.isEmpty())
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
            NX_ASSERT(false, "Item does not exist");
            continue;
        }

        const int index = it - m_devices.begin();
        const ScopedRemoveRows guard(this, QModelIndex(), index, index);

        decrementDeviceCount(m_deviceRowState[id]);
        m_deviceRowState.remove(id);
        m_devices.erase(it);
    }
}

int FoundDevicesModel::deviceCount(PresentedState presentedState) const
{
    return deviceCount(presentedState, true) + deviceCount(presentedState, false);
}

int FoundDevicesModel::deviceCount(PresentedState presentedState, bool isChecked) const
{
    return m_deviceCount[{presentedState, isChecked}];
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

    NX_ASSERT(false, "Wrong parent index");
    return 0;
}

bool FoundDevicesModel::isCorrectRow(const QModelIndex& index) const
{
    const bool correct = index.isValid() && index.row() >= 0 && index.row() < rowCount();
    if (!correct)
        NX_ASSERT(false, "Invalid index");

    return correct;
}

QnManualResourceSearchEntry FoundDevicesModel::device(const QModelIndex& index) const
{
    return isCorrectRow(index)
        ? m_devices[index.row()]
        : QnManualResourceSearchEntry();
}

QVariant FoundDevicesModel::getDisplayData(const QModelIndex& index) const
{
    if (!isCorrectRow(index))
        return QVariant();

    const auto& currentDeivce = device(index);
    switch (index.column())
    {
        case Columns::modelColumn:
            return currentDeivce.name;
        case Columns::addressColumn:
            return currentDeivce.url;
        case Columns::brandColumn:
            return currentDeivce.vendor;
        default:
            return QVariant();
    }
}

QVariant FoundDevicesModel::getCheckStateData(const QModelIndex& index) const
{
    if (!isCorrectRow(index) || index.column() != FoundDevicesModel::checkboxColumn)
        return QVariant();

    return m_deviceRowState[device(index).uniqueId].second
        ? Qt::Checked
        : Qt::Unchecked;
}

QVariant FoundDevicesModel::getForegroundColorData(const QModelIndex& index) const
{
    if (!isCorrectRow(index))
        return QVariant();

    static const auto kCheckedColor = QPalette().color(QPalette::Text);
    static const auto kRegularTextColor = QPalette().color(QPalette::Light);
    static const auto kAddedDeviceTextColor = QPalette().color(QPalette::Midlight);
    static const auto kPresentedStateColor = QPalette().color(QPalette::WindowText);

    const auto deviceRowState = m_deviceRowState[m_devices.at(index.row()).uniqueId];
    const bool isAddedDevice = deviceRowState.first == alreadyAddedState;
    const bool isChecked = deviceRowState.second;

    switch (index.column())
    {
        case brandColumn:
        case modelColumn:
        case addressColumn:
        {
            if (isAddedDevice)
                return kAddedDeviceTextColor;
            else if (isChecked)
                return kCheckedColor;
            return kRegularTextColor;
        }
        case presentedStateColumn:
        {
            return kPresentedStateColor;
        }
        case checkboxColumn:
        {
            if (isChecked)
                return kCheckedColor;
        }
    }

    return QVariant();
}

QVariant FoundDevicesModel::data(const QModelIndex& index, int role) const
{
    if (!isCorrectRow(index))
        return QVariant();

    switch (role)
    {
        case Qt::DisplayRole:
            return getDisplayData(index);
        case Qt::CheckStateRole:
            return getCheckStateData(index);
        case dataRole:
            return QVariant::fromValue(device(index));
        case presentedStateRole:
            return m_deviceRowState[device(index).uniqueId].first;
        case Qt::ForegroundRole:
            return getForegroundColorData(index);
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
        const bool newCheckState = value.value<Qt::CheckState>() != Qt::Unchecked;

        auto& deviceRowState = m_deviceRowState[uniqueId];
        auto newDeviceRowState = PresentedStateWithCheckState(deviceRowState.first, newCheckState);

        if (deviceRowState != newDeviceRowState)
        {
            decrementDeviceCount(deviceRowState);
            incrementDeviceCount(newDeviceRowState);
            deviceRowState = newDeviceRowState;
            emit dataChanged(index, index, {Qt::CheckStateRole});
            emit headerDataChanged(Qt::Horizontal,
                FoundDevicesModel::presentedStateColumn, FoundDevicesModel::presentedStateColumn);
        }
        return true;
    }
    else if (index.column() == presentedStateColumn && role == presentedStateRole)
    {
        const auto uniqueId = device(index).uniqueId;
        const PresentedState newPresentedState = value.value<PresentedState>();

        auto& deviceRowState = m_deviceRowState[uniqueId];
        auto newDeviceRowState = PresentedStateWithCheckState(newPresentedState, deviceRowState.second);

        if (deviceRowState != newDeviceRowState)
        {
            decrementDeviceCount(deviceRowState);
            incrementDeviceCount(newDeviceRowState);
            deviceRowState = newDeviceRowState;
            emit dataChanged(index, index, {presentedStateRole});
            emit headerDataChanged(Qt::Horizontal,
                FoundDevicesModel::presentedStateColumn, FoundDevicesModel::presentedStateColumn);
        }
        return true;
    }

    return base_type::setData(index, value, role);
}

QVariant FoundDevicesModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (section == Columns::presentedStateColumn)
    {
        switch(role)
        {
            case Qt::FontRole:
                return selectionColumnFont();
            case Qt::ForegroundRole:
                return QPalette().color(QPalette::Light);
            case Qt::TextAlignmentRole:
                return Qt::AlignRight;
            default:
                break;
        }
    }

    if (role != Qt::DisplayRole)
        return base_type::headerData(section, orientation, role);

    switch (section)
    {
        case Columns::brandColumn:
            return tr("Brand");
        case Columns::modelColumn:
            return tr("Model");
        case Columns::addressColumn:
            return tr("Address");
        case Columns::presentedStateColumn:
            return QString("%1, %2").arg(tr("%n device(s) total", nullptr, rowCount()))
                .arg(tr("%n new", nullptr, rowCount() - deviceCount(alreadyAddedState)));
        default:
            return QVariant();
    }
}

int FoundDevicesModel::columnCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return Columns::count;

    NX_ASSERT(false, "Wrong parent index");
    return 0;
}

Qt::ItemFlags FoundDevicesModel::flags(const QModelIndex& index) const
{
    auto result = base_type::flags(index);

    if (!isCorrectRow(index))
        return result;

    if (index.column() == checkboxColumn)
    {
        const auto presentedStateIndex = index.siblingAtColumn(presentedStateColumn);
        const auto presentedStateData = presentedStateIndex.data(presentedStateRole);
        if (!presentedStateData.isNull())
        {
            const auto presentedState = presentedStateData.value<PresentedState>();
            if (presentedState == notPresentedState)
                result.setFlag(Qt::ItemIsUserCheckable, true);
            else
                result.setFlag(Qt::ItemIsEnabled, false);
        }
    }
    return result;
}

void FoundDevicesModel::incrementDeviceCount(const PresentedStateWithCheckState& deviceState)
{
    if (!m_deviceCount.contains(deviceState))
        m_deviceCount.insert(deviceState, 1);
    else
        ++m_deviceCount[deviceState];
}

void FoundDevicesModel::decrementDeviceCount(const PresentedStateWithCheckState& deviceState)
{
    NX_ASSERT(m_deviceCount.contains(deviceState));
    --m_deviceCount[deviceState];
}

} // namespace nx::vms::client::desktop
