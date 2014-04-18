#ifndef SERVER_UPDATES_MODEL_H
#define SERVER_UPDATES_MODEL_H

#include <QtCore/QAbstractTableModel>

#include <core/resource/media_server_resource.h>
#include <utils/common/system_information.h>
#include <utils/common/software_version.h>

class QnServerUpdatesModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Columns {
        ResourceNameColumn,
        CurrentVersionColumn,
        UpdateColumn,
        ColumnCount
    };

    explicit QnServerUpdatesModel(QObject *parent = 0);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void setServers(const QList<QnMediaServerResourcePtr> &servers);
    void setUpdates(const QHash<QnSystemInformation, QnSoftwareVersion> &updates);

private:
    QList<QnMediaServerResourcePtr> m_servers;
    QHash<QnSystemInformation, QnSoftwareVersion> m_updates;
};

#endif // SERVER_UPDATES_MODEL_H
