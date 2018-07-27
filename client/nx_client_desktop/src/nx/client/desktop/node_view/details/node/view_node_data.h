#pragma once

#include <QtCore/QVariant>
#include <QtCore/QScopedPointer>

#include "../node_view_constants.h"

namespace nx {
namespace client {
namespace desktop {

class ViewNodeDataBuilder;

/**
 * @brief Represents data for Node View node.
 **/

class ViewNodeData
{
public:
    ViewNodeData();
    ViewNodeData(const ViewNodeData& other);
    ViewNodeData(const ViewNodeDataBuilder& builder);

    ~ViewNodeData();

    void applyData(const ViewNodeData& value);

    bool hasDataForColumn(int column) const;

    QVariant data(int column, int role) const;
    void setData(int column, int role, const QVariant& data);
    void removeData(int column, int role);

    QVariant commonNodeData(int role) const;
    void setCommonNodeData(int role, const QVariant& data);
    void removeCommonNodeData(int role);

    using Columns = QList<int>;
    Columns usedColumns() const;

    using Roles = QVector<int>;
    Roles rolesForColumn(int column) const;

    Qt::ItemFlags flags(int column) const;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
