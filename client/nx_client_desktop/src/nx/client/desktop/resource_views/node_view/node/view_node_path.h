#pragma once

#include <QtCore/QList>

namespace nx {
namespace client {
namespace desktop {

class ViewNodePath
{
public:
    using Indicies = QList<int>;

    ViewNodePath(const Indicies& indicies = Indicies());

    bool isEmpty() const;

    const Indicies& indicies() const;
    void appendIndex(int index);
    int leafIndex() const;

    ViewNodePath parentPath() const;

private:
    Indicies m_indicies;
};

bool operator==(const ViewNodePath& left, const ViewNodePath& right);

} // namespace desktop
} // namespace client
} // namespace nx
