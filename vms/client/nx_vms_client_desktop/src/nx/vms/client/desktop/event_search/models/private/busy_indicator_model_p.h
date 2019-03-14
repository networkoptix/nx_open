#pragma once

#include <QtCore/QAbstractListModel>

#include <client/client_globals.h>

#include <nx/utils/scoped_model_operations.h>

namespace nx::vms::client::desktop {

/** Special private model that exposes one item with value of role Qn::BusyIndicatorVisibleRole. */
class BusyIndicatorModel: public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    using base_type::base_type; //< Forward constructors.

    bool active() const;
    void setActive(bool value);

    bool visible() const;
    void setVisible(bool value);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index,
        int role = Qn::BusyIndicatorVisibleRole) const override;

private:
    bool isValid(const QModelIndex& index) const;

private:
    bool m_active = false;
    bool m_visible = true;
};

} // namespace nx::vms::client::desktop
