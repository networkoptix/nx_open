#pragma once

#include <QtCore/QVariant>
#include <QtCore/QScopedPointer>

#include "view_node_fwd.h"

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
    QVariant data(int column, int role) const;
    void setData(int column, int role, const QVariant& data);
    void removeData(int column, int role);
    bool hasDataForColumn(int column) const;
    bool hasData(int column, int role) const;

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
    using Columns = QList<int>;
    Columns usedColumns() const;

    /**
     * Returns list of roles for specified column for which instance has specified data values.
     */
    using Roles = QVector<int>;
    Roles rolesForColumn(int column) const;

    /**
     * Flag is direct representation of visual item flag.
     */
    Qt::ItemFlags flags(int column) const;
    void setFlag(int column, Qt::ItemFlag flag, bool value = true);
    void setFlags(int column, Qt::ItemFlags flags);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
