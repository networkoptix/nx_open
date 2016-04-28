#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

class QnRecentUserConnectionsModel;

//TODO: #ynikitenkov move to some common place. Btw, why are we storing passwords anyway?
typedef QPair<QString, QString> UserPasswordPair;
typedef QList<UserPasswordPair> UserPasswordPairList;

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
    typedef QHash<QString, UserPasswordPairList> DataCache;

    UnboundModelsSet m_unbound;
    BoundModelsHash m_bound;
    DataCache m_dataCache;
};
