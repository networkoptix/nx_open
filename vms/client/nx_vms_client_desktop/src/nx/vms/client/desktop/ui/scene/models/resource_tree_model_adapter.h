#pragma once

#include <ui/models/resource_tree_sort_proxy_model.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class ResourceTreeModelAdapter: public QnResourceTreeSortProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QnWorkbenchContext* context READ context WRITE setContext NOTIFY contextChanged)

    using base_type = QnResourceTreeSortProxyModel;

public:
    ResourceTreeModelAdapter(QObject* parent = nullptr);

    QnWorkbenchContext* context() const;
    void setContext(QnWorkbenchContext* context);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    static void registerQmlType();

signals:
    void contextChanged();

private:
    QnWorkbenchContext* m_context = nullptr;
};

} // nx::vms::client::desktop
