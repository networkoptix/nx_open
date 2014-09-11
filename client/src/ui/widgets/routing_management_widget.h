#ifndef ROUTING_MANAGEMENT_WIDGET_H
#define ROUTING_MANAGEMENT_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/resource_fwd.h>

#include <client/client_model_types.h>
#include <client/client_color_types.h>
#include <ui/customization/customized.h>

#include <utils/common/connective.h>

namespace Ui {
    class QnRoutingManagementWidget;
}

class QnServerAddressesModel;
class QnSortedServerAddressesModel;
class QnResourceListModel;

class QnRoutingManagementWidget : public Customized<Connective<QnAbstractPreferencesWidget>>, public QnWorkbenchContextAware {
    Q_OBJECT
    Q_PROPERTY(QnRoutingManagementColors colors READ colors WRITE setColors)
    typedef Customized<Connective<QnAbstractPreferencesWidget>> base_type;

public:
    explicit QnRoutingManagementWidget(QWidget *parent = 0);
    ~QnRoutingManagementWidget();

    virtual void updateFromSettings() override;
    virtual bool confirm() override;

    const QnRoutingManagementColors colors() const;
    void setColors(const QnRoutingManagementColors &colors);

private:
    void updateModel();
    void updateUi();

private slots:
    void at_addButton_clicked();
    void at_removeButton_clicked();
    void at_serversView_currentIndexChanged(const QModelIndex &current, const QModelIndex &previous);
    void at_addressesView_currentIndexChanged(const QModelIndex &index);
    void at_addressesView_doubleClicked(const QModelIndex &index);
    void at_serverAddressesModel_ignoreChangeRequested(const QString &address, bool ignore);

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QScopedPointer<Ui::QnRoutingManagementWidget> ui;
    QnResourceListModel *m_serverListModel;
    QnServerAddressesModel *m_serverAddressesModel;
    QnSortedServerAddressesModel *m_sortedServerAddressesModel;
    QnMediaServerResourcePtr m_server;

    QnRoutingManagementColors m_colors;
};

#endif // ROUTING_MANAGEMENT_WIDGET_H
