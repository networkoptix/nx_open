// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QCollator>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/common/models/filter_proxy_model.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace nx::utils { class PendingOperation; }

struct QnResourceSearchQuery
{
    using NodeType = nx::vms::client::desktop::ResourceTree::NodeType;

    static constexpr NodeType kAllowAllNodeTypes = NodeType::root;

    QString text;
    Qn::ResourceFlags flags;
    NodeType allowedNode = kAllowAllNodeTypes;

    QnResourceSearchQuery() = default;

    QnResourceSearchQuery(const QString& text, NodeType allowedNode):
        text(text),
        allowedNode(allowedNode)
    {
    }

    QnResourceSearchQuery(const QString& text, Qn::ResourceFlags flags = {}):
        text(text),
        flags(flags)
    {
    }

    bool operator==(const QnResourceSearchQuery& other) const
    {
        return text == other.text && flags == other.flags && allowedNode == other.allowedNode;
    }

    bool isEmpty() const
    {
        return flags == 0 && text.isEmpty();
    }
};

/**
 * A resource search filtering model.
 */
class QnResourceSearchProxyModel: public nx::vms::client::desktop::FilterProxyModel
{
    Q_OBJECT
    using base_type = nx::vms::client::desktop::FilterProxyModel;
    using NodeType = nx::vms::client::desktop::ResourceTree::NodeType;

public:
    explicit QnResourceSearchProxyModel(QObject* parent = nullptr);
    virtual ~QnResourceSearchProxyModel() override;

    virtual void setSourceModel(QAbstractItemModel* value) override;

    QnResourceSearchQuery query() const;

    // Returns new root node index according to the query's allowed node.
    QModelIndex setQuery(const QnResourceSearchQuery& query);

    enum class DefaultBehavior
    {
        showAll,
        showNone
    };

    DefaultBehavior defaultBehavor() const;
    void setDefaultBehavior(DefaultBehavior value);

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool isAcceptedIndex(QModelIndex index, NodeType allowedNode) const;
    bool isRejectedNodeType(NodeType nodeType, NodeType allowedNodeType) const;
    bool isResourceMatchesQuery(QnResourcePtr resource, const QnResourceSearchQuery& query) const;
    bool isRepresentationMatchesQuery(const QModelIndex& index,
        const QnResourceSearchQuery& query) const;

private:
    QnResourceSearchQuery m_query;
    QPersistentModelIndex m_currentRootNode;
    DefaultBehavior m_defaultBehavior = DefaultBehavior::showAll;
    QList<QPersistentModelIndex> m_parentIndicesToUpdate;
    const QScopedPointer<nx::utils::PendingOperation> m_updateParentsOp;
};
