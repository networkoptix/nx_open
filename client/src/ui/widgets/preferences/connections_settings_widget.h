#ifndef CONNECTIONSSETTINGSWIDGET_H
#define CONNECTIONSSETTINGSWIDGET_H

#include <QtCore/QList>
#include <QtCore/QUrl>

#include <QtGui/QWidget>

#include "utils/settings.h"

class QDataWidgetMapper;
class QModelIndex;
class QStandardItem;
class QStandardItemModel;

namespace Ui {
    class QnConnectionsSettingsWidget;
}

class QnConnectionsSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QnConnectionsSettingsWidget(QWidget *parent = 0);
    ~QnConnectionsSettingsWidget();

    QList<QnSettings::ConnectionData> connections() const;
    void setConnections(const QList<QnSettings::ConnectionData> &connections);
    void addConnection(const QnSettings::ConnectionData &connection);
    void removeConnection(const QnSettings::ConnectionData &connection);

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

#endif // CONNECTIONSSETTINGSWIDGET_H
