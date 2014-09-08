#ifndef SERVER_UPDATES_MODEL_H
#define SERVER_UPDATES_MODEL_H

#include <QtCore/QAbstractTableModel>

#include <core/resource/media_server_resource.h>
#include <utils/common/system_information.h>
#include <utils/common/software_version.h>

#include <update/media_server_update_tool.h>

class QnServerUpdatesModel : public QAbstractTableModel {
    Q_OBJECT

    typedef QAbstractTableModel base_type;
public:
    enum Columns {
        NameColumn,
        VersionColumn,
        ColumnCount
    };

    enum Roles {
        StageRole = Qn::LastItemDataRole + 1,
        ProgressRole
    };

    explicit QnServerUpdatesModel(QnMediaServerUpdateTool* tool, QObject *parent = 0);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(const QnMediaServerResourcePtr &server) const;
    QModelIndex index(const QUuid &id) const;

    QnCheckForUpdateResult checkResult() const;
    void setCheckResult(const QnCheckForUpdateResult &result);

public slots:
    void setTargets(const QSet<QUuid> &targets);

private:
    void resetResourses();

private slots:
    void at_resourceChanged(const QnResourcePtr &resource);

private:
    class Item {
    public:
        Item(const QnMediaServerResourcePtr &server) :
            m_server(server)
        {}

        QnMediaServerResourcePtr server() const;
        QnPeerUpdateStage stage() const;

        QVariant data(int column, int role) const;

    private:
        QnMediaServerResourcePtr m_server;
        QnPeerUpdateStage m_stage;
        int m_progress;

        friend class QnServerUpdatesModel;
    };

    QnMediaServerUpdateTool* m_updateTool;
    QList<Item*> m_items;
    QSet<QUuid> m_targets;

    QnCheckForUpdateResult m_checkResult;
};

#endif // SERVER_UPDATES_MODEL_H
