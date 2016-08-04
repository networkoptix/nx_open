#pragma once

#include <QtCore/QIdentityProxyModel>
#include <core/resource_management/resource_access_provider.h>

/*
* Proxy model that works with QnResourceListModel (or its proxy) as a source
*  and inserts a virtual column that displays an icon of a resource through
*  which a user gains implicit access to the resource of corresponding row.
*/
class QnAccessibleResourcesModel : public QIdentityProxyModel
{
    using base_type = QIdentityProxyModel;

public:
    enum Column
    {
        NameColumn,
        IndirectAccessColumn,
        CheckColumn,
        ColumnCount
    };

    explicit QnAccessibleResourcesModel(QObject* parent = nullptr);

    virtual ~QnAccessibleResourcesModel();

    using IndirectAccessFunction = std::function<QnIndirectAccessProviders()>;
    IndirectAccessFunction indirectAccessFunction() const;
    void setIndirectAccessFunction(IndirectAccessFunction function);

    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex sibling(int row, int column, const QModelIndex& idx) const override;

    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool allChecked() const;
    void setAllChecked(bool value);

public slots:
    void indirectAccessChanged();

private:
    using IndirectAccessInfo = QPair<QIcon, QString>;
    IndirectAccessInfo indirectAccessInfo(const QnResourcePtr& resource) const;

private:
    bool m_allChecked;
    IndirectAccessFunction m_indirectAccessFunction;

    mutable QHash<QnResourcePtr, IndirectAccessInfo> m_indirectAccessInfoCache;
    mutable QnIndirectAccessProviders m_indirectlyAccessibleResources;
    mutable bool m_indirectlyAccessibleDirty;
};
