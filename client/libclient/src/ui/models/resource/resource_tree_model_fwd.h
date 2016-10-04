#pragma once

#include <core/resource/shared_resource_pointer.h>

class QnResourceTreeModel;
class QnResourceTreeModelNode;
class QnResourceTreeModelUserNodes;

typedef QnSharedResourcePointer<QnResourceTreeModelNode> QnResourceTreeModelNodePtr;

typedef QHash<QString, QnResourceTreeModelNodePtr> RecorderHash;
typedef QHash<QnUuid, QnResourceTreeModelNodePtr> ItemHash;
typedef QHash<QnResourcePtr, QnResourceTreeModelNodePtr> ResourceHash;
typedef QList<QnResourceTreeModelNodePtr> NodeList;
