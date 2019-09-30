#pragma once

#include <ui/models/resource_search_proxy_model.h>

class QnResourceTreeSortProxyModel: public QnResourceSearchProxyModel
{
    typedef QnResourceSearchProxyModel base_type;

public:
    NX_VMS_CLIENT_DESKTOP_API QnResourceTreeSortProxyModel(QObject* parent = nullptr);

    virtual bool setData(
        const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    virtual Qt::DropActions supportedDropActions() const override;

private:
    /**
     * Helper function to list nodes in the correct order.
     * Root nodes are strictly ordered, but there is one type of node which is inserted in between:
     * videowall nodes, which are pinned between Layouts and WebPages. Also if the system has one
     * server, ServersNode is not displayed, so server/edge node must be displayed on it's place.
     */
    static qreal nodeOrder(const QModelIndex& index);

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
