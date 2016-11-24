#pragma once

#include <client/client_globals.h>

#include <ui/models/resource/resource_tree_model_fwd.h>

class QnResourceTreeModelNodeFactory
{
public:
    static QnResourceTreeModelNodePtr createNode(Qn::NodeType nodeType, QnResourceTreeModel* model);
};
