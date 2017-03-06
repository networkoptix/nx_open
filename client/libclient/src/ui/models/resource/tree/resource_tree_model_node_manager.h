#pragma once

#include <ui/models/resource/resource_tree_model_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnResourceTreeModelNodeManager: public Connective<QObject>, public QnWorkbenchContextAware
{
    using base_type = Connective<QObject>;

public:
    QnResourceTreeModelNodeManager(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModelNodeManager();

protected:
    template<class Type = QnResourceTreeModelNode, class Method, class ... Args>
    static void chainCall(QnResourceTreeModelNode* from, Method method, Args ... args)
    {
        for (auto node = from; node != nullptr; node = node->m_next)
        {
            if (auto typedNode = qobject_cast<Type*>(node))
                (typedNode->*method)(args...);
            else
                NX_ASSERT(false);
        }
    }

    template<class Type = QnResourceTreeModelNode, class Method, class ... Args>
    void resourceChainCall(const QnResourcePtr& resource, Method method, Args ... args)
    {
        chainCall<Type>(m_primaryResourceNodes[resource], method, args...);
    }

protected:
    virtual void primaryNodeAdded(QnResourceTreeModelNode* node);
    virtual void primaryNodeRemoved(QnResourceTreeModelNode* node);

private:
    /* Called from QnResourceTreeModelNode::setResource: */
    void addResourceNode(QnResourceTreeModelNode* node);
    void removeResourceNode(QnResourceTreeModelNode* node);

protected:
    friend class QnResourceTreeModelNode;
    QHash<QnResourcePtr, QnResourceTreeModelNode*> m_primaryResourceNodes;
};
