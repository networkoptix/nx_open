#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

#include <core/user_recent_connection_data.h>

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
    typedef QSet<QnRecentUserConnectionsModel*> UnboundModelsSet;
    typedef QHash<QString, QnRecentUserConnectionsModel*> BoundModelsHash;
    typedef QHash<QString, QnUserRecentConnectionDataList> DataCache;

    UnboundModelsSet m_unbound;
    BoundModelsHash m_bound;
    DataCache m_dataCache;
};
