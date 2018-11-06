#include "busy_indicator_model_p.h"

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

namespace {

QString debugId(BusyIndicatorModel* model)
{
    const auto name = model->objectName();
    if (name.isEmpty())
        return {};

    return QString("[%1]").arg(name);
}

} // namespace

bool BusyIndicatorModel::active() const
{
    return m_active;
}

void BusyIndicatorModel::setActive(bool value)
{
    if (m_active == value)
        return;

    NX_VERBOSE(this) << "Setting active to" << value << debugId(this);

    m_active = value;
    if (!m_visible)
        return;

    static const QVector<int> kBusyIndicatorVisibleRole({Qn::BusyIndicatorVisibleRole});
    emit dataChanged(index(0), index(0), kBusyIndicatorVisibleRole);
}

bool BusyIndicatorModel::visible() const
{
    return m_visible;
}

void BusyIndicatorModel::setVisible(bool value)
{
    if (m_visible == value)
        return;

    NX_VERBOSE(this) << "Setting visible to" << value <<  debugId(this);

    ScopedReset reset(this);
    m_visible = value;
}

int BusyIndicatorModel::rowCount(const QModelIndex& parent) const
{
    return (m_visible && !parent.isValid()) ? 1 : 0;
}

QVariant BusyIndicatorModel::data(const QModelIndex& index, int role) const
{
    return role == Qn::BusyIndicatorVisibleRole && isValid(index)
        ? QVariant::fromValue(m_active)
        : QVariant();
}

bool BusyIndicatorModel::isValid(const QModelIndex& index) const
{
    return m_visible && index.column() == 0 && index.row() == 0
        && index.model() == this && !index.parent().isValid();
}

} // namespace nx::vms::client::desktop
