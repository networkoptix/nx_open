#ifndef QN_CONNECTIONS_SETTINGS_WIDGET_H
#define QN_CONNECTIONS_SETTINGS_WIDGET_H

#include <QtCore/QList>
#include <QtCore/QUrl>

#include <QtGui/QWidget>

#include "utils/settings.h"

class QDataWidgetMapper;
class QModelIndex;
class QStandardItem;
class QStandardItemModel;

namespace Ui {
    class ConnectionsSettingsWidget;
}

class QnConnectionsSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnConnectionsSettingsWidget(QWidget *parent = 0);
    ~QnConnectionsSettingsWidget();

    QnConnectionDataList connections() const;
    void setConnections(const QnConnectionDataList &connections);
    void addConnection(const QnConnectionData &connection);
    void removeConnection(const QnConnectionData &connection);

protected:
    void changeEvent(QEvent *event);

    void retranslateUi();

private Q_SLOTS:
    void currentRowChanged(const QModelIndex &current, const QModelIndex &previous);
    void savePasswordToggled(bool save);
    void newConnection();
    void testConnection();
    void duplicateConnection();
    void deleteConnection();

private:
    Q_DISABLE_COPY(QnConnectionsSettingsWidget)

    QUrl currentUrl();

    QScopedPointer<Ui::ConnectionsSettingsWidget> ui;

    QStandardItemModel *m_connectionsModel;
    QStandardItem *m_connectionsRootItem;

    QDataWidgetMapper *m_dataWidgetMapper;
};

#endif // QN_CONNECTIONS_SETTINGS_WIDGET_H
