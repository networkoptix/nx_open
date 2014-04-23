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

    enum UpdateStatus {
        NotFound,
        Found,
        Uploading,
        Installing,
        Installed
    };

    struct UpdateInformation {
        UpdateStatus status;
        QnSoftwareVersion version;
        int progress;

        UpdateInformation() : progress(0) {}
    };
    typedef QHash<QnSystemInformation, UpdateInformation> UpdateInformationHash;

    explicit QnServerUpdatesModel(QObject *parent = 0);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QList<QnMediaServerResourcePtr> servers() const;
    void setServers(const QList<QnMediaServerResourcePtr> &servers);
    void setUpdatesInformation(const UpdateInformationHash &updates);

private:
    QList<QnMediaServerResourcePtr> m_servers;
    UpdateInformationHash m_updates;
};

#endif // SERVER_UPDATES_MODEL_H
