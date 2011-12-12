#ifndef CONNECTIONSSETTINGSWIDGET_H
#define CONNECTIONSSETTINGSWIDGET_H

#include <QtCore/QList>
#include <QtCore/QUrl>

#include <QtGui/QWidget>

class QDataWidgetMapper;
class QModelIndex;
class QStandardItem;
class QStandardItemModel;

namespace Ui {
    class ConnectionsSettingsWidget;
}

class ConnectionsSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionsSettingsWidget(QWidget *parent = 0);
    ~ConnectionsSettingsWidget();

    struct ConnectionData {
        inline bool operator==(const ConnectionData &other) const
        { return name == other.name && url == other.url; }

        QString name;
        QUrl url;
        // ### QString advancedParameters;
    };

    QList<ConnectionData> connections() const;
    void setConnections(const QList<ConnectionData> &connections);
    void addConnection(const ConnectionData &connection);
    void removeConnection(const ConnectionData &connection);

protected:
    void changeEvent(QEvent *event);

    void retranslateUi();

private Q_SLOTS:
    void currentRowChanged(const QModelIndex &current, const QModelIndex &previous);
    void savePasswordToggled(bool save);
    void newConnection();
    void duplicateConnection();
    void deleteConnection();

private:
    Q_DISABLE_COPY(ConnectionsSettingsWidget)

    QScopedPointer<Ui::ConnectionsSettingsWidget> ui;

    QStandardItemModel *m_connectionsModel;
    QStandardItem *m_connectionsRootItem;

    QDataWidgetMapper *m_dataWidgetMapper;
};

#endif // CONNECTIONSSETTINGSWIDGET_H
