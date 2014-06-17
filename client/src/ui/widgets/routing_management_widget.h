#ifndef ROUTING_MANAGEMENT_WIDGET_H
#define ROUTING_MANAGEMENT_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/resource_fwd.h>

namespace Ui {
    class QnRoutingManagementWidget;
}

class QnServerAddressesModel;
class QnSortedServerAddressesModel;

class QnRoutingManagementWidget : public QWidget, public QnWorkbenchContextAware {
    Q_OBJECT

public:
    explicit QnRoutingManagementWidget(QWidget *parent = 0);
    ~QnRoutingManagementWidget();

private:
    QnMediaServerResourcePtr currentServer() const;
    void updateModel(const QnMediaServerResourcePtr &server);

private slots:
    void at_addButton_clicked();
    void at_removeButton_clicked();
    void at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous);
    void at_addressesView_currentIndexChanged(const QModelIndex &index);
    void at_addressesView_doubleClicked(const QModelIndex &index);
    void at_currentServer_changed(const QnResourcePtr &resource);
    void at_serverAddressesModel_ignoreChangeRequested(const QString &address, bool ignore);

private:
    QScopedPointer<Ui::QnRoutingManagementWidget> ui;
    QnServerAddressesModel *m_serverAddressesModel;
    QnSortedServerAddressesModel *m_sortedServerAddressesModel;
};

#endif // ROUTING_MANAGEMENT_WIDGET_H
