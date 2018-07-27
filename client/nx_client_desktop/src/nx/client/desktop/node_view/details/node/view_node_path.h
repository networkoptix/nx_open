#pragma once

#include <QtCore/QList>

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath
{
public:
    using Indices = QList<int>;

    ViewNodePath(const Indices& indices = Indices());

    bool isEmpty() const;

    const Indices& indices() const;
    void appendIndex(int index);
    int lastIndex() const;

    ViewNodePath parentPath() const;

private:
    Indices m_indices;
};

bool operator==(const ViewNodePath& left, const ViewNodePath& right);

} // namespace desktop
} // namespace client
} // namespace nx
