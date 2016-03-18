#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

class QnLastSystemUsersModel; //TODO: #ynikitenkov rename it

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

    void addModel(QnLastSystemUsersModel* model);
    void removeModel(QnLastSystemUsersModel* model);

private:
    void updateModelBinding(QnLastSystemUsersModel* model);
    void updateModelsData();

private:
    typedef QSet<QnLastSystemUsersModel*> UnboundModelsSet;
    typedef QHash<QString, QnLastSystemUsersModel*> BoundModelsHash;
    typedef QHash<QString, UserPasswordPairList> DataCache;

    UnboundModelsSet m_unbound;
    BoundModelsHash m_bound;
    DataCache m_dataCache;
};
