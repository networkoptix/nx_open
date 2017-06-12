#pragma once

#include <QtCore/QSortFilterProxyModel>

#include <client_core/connection_context_aware.h>

#include <core/resource_access/resource_access_subject.h>

/*
* Proxy model that works with QnResourceListModel (or its proxy) as a source
*  and inserts a virtual column that displays an icon of a resource through
*  which a user gains implicit access to the resource of corresponding row.
*/
class QnAccessibleResourcesModel : public QSortFilterProxyModel, public QnConnectionContextAware
{
    //TODO: #vkutin Implement "QnRelayoutColumnsProxyModel" and use it as a base
    using base_type = QSortFilterProxyModel;

    Q_OBJECT
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

    QnResourceAccessSubject subject() const;
    void setSubject(const QnResourceAccessSubject& subject);


    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    virtual QModelIndex sibling(int row, int column, const QModelIndex& idx) const override;
    virtual QModelIndex parent(const QModelIndex& index) const override;

    virtual QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool allChecked() const;
    void setAllChecked(bool value);

private:
    QString getTooltip(const QnResourceList& providers) const;

    void accessChanged();

private:
    bool m_allChecked;
    QnResourceAccessSubject m_subject;
};
