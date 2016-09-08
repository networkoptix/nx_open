#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/singleton.h>

#include <client_core/user_recent_connection_data.h>

class QnRecentUserConnectionsModel;

class QnClientRecentConnectionsManager: public QObject, public Singleton<QnClientRecentConnectionsManager>
{
    typedef QObject base_type;
    Q_OBJECT

public:
    QnClientRecentConnectionsManager();
    virtual ~QnClientRecentConnectionsManager();

    void addModel(QnRecentUserConnectionsModel* model);
    void removeModel(QnRecentUserConnectionsModel* model);

private:
    void updateModelBinding(QnRecentUserConnectionsModel* model);
    void updateModelsData();

private:
    using QnRecentUserConnectionsModelPtr = QPointer<QnRecentUserConnectionsModel>;
    using DataCache = QHash<QString, QnUserRecentConnectionDataList>;

    QList<QnRecentUserConnectionsModelPtr> m_models;
    DataCache m_dataCache;
    bool m_updating;
};
