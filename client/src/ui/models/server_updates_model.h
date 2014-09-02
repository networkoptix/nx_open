#ifndef SERVER_UPDATES_MODEL_H
#define SERVER_UPDATES_MODEL_H

#include <QtCore/QAbstractTableModel>

#include <core/resource/media_server_resource.h>
#include <utils/common/system_information.h>
#include <utils/common/software_version.h>
#include <utils/media_server_update_tool.h>

class QnServerUpdatesModel : public QAbstractTableModel {
    Q_OBJECT

    typedef QAbstractTableModel base_type;
public:

    enum Columns {
        ResourceNameColumn,
        CurrentVersionColumn,
        UpdateColumn,
        LastColumn = UpdateColumn,
        ColumnCount
    };

    enum Roles {
        StateRole = Qt::UserRole + 1,
        ProgressRole
    };

    class Item {
    public:
        Item(const QnMediaServerResourcePtr &server, const QnMediaServerUpdateTool::PeerUpdateInformation &updateInfo) :
            m_server(server), m_updateInfo(updateInfo)
        {}

        QnMediaServerResourcePtr server() const;
        QnMediaServerUpdateTool::PeerUpdateInformation updateInformation() const;

        QVariant data(int column, int role) const;

    private:
        QnMediaServerResourcePtr m_server;
        QnMediaServerUpdateTool::PeerUpdateInformation m_updateInfo;

        friend class QnServerUpdatesModel;
    };

    explicit QnServerUpdatesModel(QObject *parent = 0);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(const QnMediaServerResourcePtr &server) const;

    QnMediaServerResourceList servers() const;
    void setUpdatesInformation(const QHash<QUuid, QnMediaServerUpdateTool::PeerUpdateInformation> &updates);
    void setUpdateInformation(const QnMediaServerUpdateTool::PeerUpdateInformation &update);

private:
    void resetResourses();

private slots:
    void at_resourceAdded(const QnResourcePtr &resource);
    void at_resourceRemoved(const QnResourcePtr &resource);
    void at_resourceChanged(const QnResourcePtr &resource);

private:
    QList<Item*> m_items;
    QHash<QUuid, QnMediaServerUpdateTool::PeerUpdateInformation> m_updates;
};

#endif // SERVER_UPDATES_MODEL_H
