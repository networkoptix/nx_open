#ifndef ROUTING_MANAGEMENT_WIDGET_H
#define ROUTING_MANAGEMENT_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtNetwork/QHostAddress>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>
#include <utils/common/id.h>

namespace Ui {
    class QnRoutingManagementWidget;
}

class QnServerAddressesModel;
class QnSortedServerAddressesModel;
class QnResourceListModel;

class QnRoutingManagementWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    explicit QnRoutingManagementWidget(QWidget *parent = 0);
    ~QnRoutingManagementWidget();

    virtual void updateFromSettings() override;
    virtual void submitToSettings() override;

private:
    void updateModel();
    void updateFromModel();
    void updateUi();

private slots:
    void at_addButton_clicked();
    void at_removeButton_clicked();
    void at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous);
//    void at_addressesView_doubleClicked(const QModelIndex &index);
//    void at_serverAddressesModel_ignoreChangeRequested(const QString &address, bool ignore);

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QScopedPointer<Ui::QnRoutingManagementWidget> ui;
    QnResourceListModel *m_serverListModel;
    QnServerAddressesModel *m_serverAddressesModel;
    QnSortedServerAddressesModel *m_sortedServerAddressesModel;
    QnMediaServerResourcePtr m_server;
    QMultiHash<QnUuid, QHostAddress> m_autoAddressesByServerId;
    QMultiHash<QnUuid, QUrl> m_additionalUrlsByServerId;
    QMultiHash<QnUuid, QUrl> m_ignoredUrlsByServerId;
};

#endif // ROUTING_MANAGEMENT_WIDGET_H
