// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef ROUTING_MANAGEMENT_WIDGET_H
#define ROUTING_MANAGEMENT_WIDGET_H

#include <QtNetwork/QHostAddress>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/id.h>

namespace Ui {
    class QnRoutingManagementWidget;
}

class QnServerAddressesModel;
class QnSortedServerAddressesModel;
class QnResourceListModel;

class RoutingManagementChanges;

class QnRoutingManagementWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    explicit QnRoutingManagementWidget(QWidget *parent = 0);
    ~QnRoutingManagementWidget();

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool hasChanges() const override;
    virtual bool isNetworkRequestRunning() const override;

protected:
    virtual void setReadOnlyInternal(bool readOnly) override;

private:
    void updateModel();
    void updateFromModel();
    void updateUi();

    quint16 getCurrentServerPort();

private slots:
    void at_addButton_clicked();
    void at_removeButton_clicked();
    void at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous);
    void at_serverAddressesModel_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void reportUrlEditingError(int error);

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QScopedPointer<Ui::QnRoutingManagementWidget> ui;
    QScopedPointer<RoutingManagementChanges> m_changes;
    QnResourceListModel *m_serverListModel;
    QnServerAddressesModel *m_serverAddressesModel;
    QnSortedServerAddressesModel *m_sortedServerAddressesModel;
    QnMediaServerResourcePtr m_server;
    QSet<int> m_requests;
};

#endif // ROUTING_MANAGEMENT_WIDGET_H
