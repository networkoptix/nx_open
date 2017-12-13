#include "busy_indicator_model_p.h"

namespace nx {
namespace client {
namespace desktop {

bool BusyIndicatorModel::active() const
{
    return m_active;
}

bool BusyIndicatorModel::setActive(bool value)
{
    if (m_active == value)
        return false;

    m_active = value;

    static const QVector<int> kBusyIndicatorVisibleRole = {Qn::BusyIndicatorVisibleRole};
    emit dataChanged(index(0), index(0), kBusyIndicatorVisibleRole);

    return true;
}

int BusyIndicatorModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant BusyIndicatorModel::data(const QModelIndex& index, int role) const
{
    return isValid(index) && role == Qn::BusyIndicatorVisibleRole
        ? QVariant::fromValue(m_active)
        : QVariant();
}

bool BusyIndicatorModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    return isValid(index) && role == Qn::BusyIndicatorVisibleRole && value.canConvert<bool>()
        ? setActive(value.toBool())
        : false;
}

bool BusyIndicatorModel::isValid(const QModelIndex& index) const
{
    return index.model() == this && !index.parent().isValid()
        && index.column() == 0 && index.row() == 0;
}

} // namespace desktop
} // namespace client
} // namespace nx
