#pragma once

#include "view_node_fwd.h"
#include  "../node_view_state_patch.h"

#include <QtCore/QVariant>
#include <QtCore/QScopedPointer>

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

/**
 * Represents data storage for view node. Contains raw QVariant data for column/role keys.
 * Supports generic properties applied for whole node. Contains flags for visual item.
 * Used for node describing and construction.
 */
class NX_VMS_CLIENT_DESKTOP_API ViewNodeData
{
public:

public:
    ViewNodeData();
    ViewNodeData(const ViewNodeData& other);
    ViewNodeData(const ViewNodeDataBuilder& builder);

    ~ViewNodeData();

    ViewNodeData& operator=(const ViewNodeData& other);

    /**
     * Sets any data/property/flag that is found in 'value' argument to current instance.
     */
    void applyData(const ViewNodeData& value);

    /**
     * Data represents raw QVariant values for specified column/role. Used directly in model.
     */
    QVariant data(Column column, Role role) const;
    void setData(Column column, Role role, const QVariant& data);
    void removeData(Column column, Role role);
    void removeData(const ColumnRoleHash& rolesHash);
    bool hasDataForColumn(Column column) const;
    bool hasData(Column column, Role role) const;

    /**
     * Property represents generic data that applies to whole node.
     */
    QVariant property(int id) const;
    void setProperty(int id, const QVariant& data);
    void removeProperty(int id);
    bool hasProperty(int id) const;

    /**
     * Returns list of columns for which instance has specified data values.
     */
    ColumnSet usedColumns() const;

    /**
     * Returns list of roles for specified column for which instance has specified data values.
     */
    RoleVector rolesForColumn(Column column) const;

    /**
     * Flag is direct representation of visual item flag.
     */
    Qt::ItemFlags flags(Column column) const;
    void setFlag(Column column, Qt::ItemFlag flag, bool value = true);
    void setFlags(Column column, Qt::ItemFlags flags);

    // TODO: 4.2 #ynikitenkov Develop unit tests.

    struct DifferenceData
    {
        OperationData removeOperation;
        OperationData updateOperation;
    };

    /**
     * Returns operations witch should be applied to current data to become "other" one.
     */
    DifferenceData difference(const ViewNodeData& other) const;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::node_view::details::ViewNodeData)
