#pragma once

#include <QtCore/QVariant>
#include <QtCore/QScopedPointer>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

class ViewNodeDataBuilder;

class ViewNodeData
{
public:
    ViewNodeData();
    ViewNodeData(const ViewNodeData& other);
    ViewNodeData(const ViewNodeDataBuilder& builder);

    ~ViewNodeData();

    void applyData(const ViewNodeData& value);

    bool hasDataForColumn(int column) const;
    bool hasData(int column, int role) const;
    QVariant data(int column, int role) const;
    void setData(int column, int role, const QVariant& data);
    void removeData(int column, int role);

    bool hasProperty(int id) const;
    QVariant property(int id) const;
    void setProperty(int id, const QVariant& data);
    void removeProperty(int id);

    using Columns = QList<int>;
    Columns usedColumns() const;

    using Roles = QVector<int>;
    Roles rolesForColumn(int column) const;

    Qt::ItemFlags flags(int column) const;
    void setFlag(int column, Qt::ItemFlag flag, bool value = true);
    void setFlags(int column, Qt::ItemFlags flags);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx
