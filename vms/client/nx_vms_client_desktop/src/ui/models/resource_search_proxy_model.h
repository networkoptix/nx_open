#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QSortFilterProxyModel>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <ui/models/resource/resource_compare_helper.h>

#include <nx/vms/client/desktop/resource_views/data/node_type.h>

struct QnResourceSearchQuery
{
    using NodeType = nx::vms::client::desktop::ResourceTreeNodeType;

    static constexpr NodeType kAllowAllNodeTypes = NodeType::root;

    QString text;
    Qn::ResourceFlags flags = 0;
    NodeType allowedNode = kAllowAllNodeTypes;

    QnResourceSearchQuery() = default;

    QnResourceSearchQuery(const QString& text, NodeType allowedNode):
        text(text),
        allowedNode(allowedNode)
    {
    }

    QnResourceSearchQuery(const QString& text, Qn::ResourceFlags flags = 0):
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
class QnResourceSearchProxyModel: public QSortFilterProxyModel, protected QnResourceCompareHelper
{
    Q_OBJECT
    using base_type = QSortFilterProxyModel;
    using NodeType = nx::vms::client::desktop::ResourceTreeNodeType;

public:
    explicit QnResourceSearchProxyModel(QObject* parent = nullptr);
    virtual ~QnResourceSearchProxyModel() override;

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

    // --------------------------------------------------------------
    // Add a override function for lessThan to achieve customization
    // --------------------------------------------------------------
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

signals:
    void criteriaChanged();

    /**
     * This signal is emitted when the tree prepares to start a recursive operation
     * that may lead to a lot of dataChanged signals emitting.
     */
    void beforeRecursiveOperation();

    /**
     * This signal is emitted when the tree ends a recursive operation.
     */
    void afterRecursiveOperation();

public slots:
    void invalidateFilter();

    /**
     * Performs delayed invalidation of the filter.
     *
     * The filter will be invalidated only once even if this function was
     * called several times.
     */
    void invalidateFilterLater();

protected:
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool isVisibleNodeType(NodeType nodeType, bool isSearchMode) const;
    bool isResourceMatchesQuery(QnResourcePtr resource, const QnResourceSearchQuery& query) const;
    bool isRepresentationMatchesQuery(const QModelIndex& index,
        const QnResourceSearchQuery& query) const;

private:
    bool m_invalidating = false;
    QnResourceSearchQuery m_query;
    QModelIndex m_currentRootNode;
    DefaultBehavior m_defaultBehavior = DefaultBehavior::showAll;
};

Q_DECLARE_METATYPE(QnResourceSearchProxyModel*)
