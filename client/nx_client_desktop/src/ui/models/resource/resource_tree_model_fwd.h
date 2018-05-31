#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/shared_resource_pointer.h>

class QnResourceTreeModel;
class QnResourceTreeModelNode;
class QnResourceTreeModelNodeManager;

typedef QnSharedResourcePointer<QnResourceTreeModelNode> QnResourceTreeModelNodePtr;

typedef QHash<QString, QnResourceTreeModelNodePtr> RecorderHash;
typedef QHash<QnUuid, QnResourceTreeModelNodePtr> ItemHash;
typedef QHash<QnResourcePtr, QnResourceTreeModelNodePtr> ResourceHash;
typedef QList<QnResourceTreeModelNodePtr> NodeList;

namespace nx {
namespace client {
namespace desktop {

enum class ResourceTreeNodeType;

} // namespace desktop
} // namespace client
} // namespace nx
