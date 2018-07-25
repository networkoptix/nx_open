#pragma once

#include <QtCore/QVariant>
#include <QtCore/QScopedPointer>

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

    bool hasData(int column, int role) const;
    bool hasDataForColumn(int column) const;

    QVariant data(int column, int role) const;
    void setData(int column, int role, const QVariant& data);
    void removeData(int column, int role);

    QVariant genericData(int role) const;
    void setGenericData(int role, const QVariant& data);
    void removeGenericData(int role);

    using Columns = QList<int>;
    Columns columns() const;

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
