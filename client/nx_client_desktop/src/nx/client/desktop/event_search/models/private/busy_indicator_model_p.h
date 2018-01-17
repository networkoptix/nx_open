#pragma once

#include <QtCore/QAbstractListModel>

#include <client/client_globals.h>

namespace nx {
namespace client {
namespace desktop {

/** Special private model that exposes one item with value of role Qn::BusyIndicatorVisibleRole. */
class BusyIndicatorModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    using base_type::base_type; //< Forward constructors.

    bool active() const;
    bool setActive(bool value); //< Returns true if activity was changed.

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index,
        int role = Qn::BusyIndicatorVisibleRole) const override;

    virtual bool setData(const QModelIndex& index, const QVariant& value,
        int role = Qn::BusyIndicatorVisibleRole) override;

private:
    bool isValid(const QModelIndex& index) const;

private:
    bool m_active = false;
};

} // namespace desktop
} // namespace client
} // namespace nx
