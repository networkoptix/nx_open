#pragma once

#include <nx/client/desktop/common/models/column_remap_proxy_model.h>

#include <client_core/connection_context_aware.h>

#include <core/resource_access/resource_access_subject.h>

/*
* Proxy model that works with QnResourceListModel (or its proxy) as a source
*  and inserts a virtual column that displays an icon of a resource through
*  which a user gains implicit access to the resource of corresponding row.
*/
class QnAccessibleResourcesModel:
    public nx::client::desktop::ColumnRemapProxyModel,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = nx::client::desktop::ColumnRemapProxyModel;

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
