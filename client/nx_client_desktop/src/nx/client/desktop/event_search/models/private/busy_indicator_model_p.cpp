#include "busy_indicator_model_p.h"

namespace nx {
namespace client {
namespace desktop {

bool BusyIndicatorModel::active() const
{
    return m_active;
}

void BusyIndicatorModel::setActive(bool value)
{
    if (m_active == value)
        return;

    ScopedReset reset(this);
    m_active = value;
}

int BusyIndicatorModel::rowCount(const QModelIndex& parent) const
{
    return (m_active && !parent.isValid()) ? 1 : 0;
}

QVariant BusyIndicatorModel::data(const QModelIndex& index, int role) const
{
    return isValid(index) && role == Qn::BusyIndicatorVisibleRole
        ? QVariant::fromValue(true)
        : QVariant();
}

bool BusyIndicatorModel::isValid(const QModelIndex& index) const
{
    return index.model() == this && !index.parent().isValid()
        && index.column() == 0 && index.row() == 0;
}

} // namespace desktop
} // namespace client
} // namespace nx
