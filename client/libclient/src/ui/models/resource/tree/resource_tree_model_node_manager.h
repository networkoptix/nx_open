#pragma once

#include <ui/models/resource/resource_tree_model_node.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnResourceTreeModelNodeManager: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnResourceTreeModelNodeManager(QnResourceTreeModel* model);
    virtual ~QnResourceTreeModelNodeManager();

protected:
    template<class Type = QnResourceTreeModelNode, class Method, class ... Args>
    static void chainCall(QnResourceTreeModelNode* from, Method method, Args ... args)
    {
        for (auto node = from; node != nullptr; node = node->m_next.data())
        {
            NX_ASSERT(node->m_initialized);
            if (auto typedNode = qobject_cast<Type*>(node))
                (typedNode->*method)(args...);
            else
                NX_ASSERT(false);
        }
    }

    template<class Type = QnResourceTreeModelNode, class Method, class ... Args>
    void resourceChainCall(const QnResourcePtr& resource, Method method, Args ... args)
    {
        chainCall<Type>(nodeForResource(resource).data(), method, args...);
    }

protected:
    using PrimaryResourceNodes = QHash<QnResourcePtr, QnResourceTreeModelNodePtr>;
    const PrimaryResourceNodes& primaryResourceNodes() const;

    QnResourceTreeModelNodePtr nodeForResource(const QnResourcePtr& resource) const;

    virtual void primaryNodeAdded(QnResourceTreeModelNode* node);
    virtual void primaryNodeRemoved(QnResourceTreeModelNode* node);

private:
    /* Called from QnResourceTreeModelNode::setResource: */
    void addResourceNode(const QnResourceTreeModelNodePtr& node);
    void removeResourceNode(const QnResourceTreeModelNodePtr& node);

private:
    friend class QnResourceTreeModelNode;

    /* This collection contains only nodes handled by this manager instance. */
    PrimaryResourceNodes m_primaryResourceNodes;
};
