#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <client_core/local_connection_data.h>

class QnRecentLocalConnectionsModel;

class QnClientRecentConnectionsManager: public QObject, public Singleton<QnClientRecentConnectionsManager>
{
    typedef QObject base_type;
    Q_OBJECT

public:
    QnClientRecentConnectionsManager();
    virtual ~QnClientRecentConnectionsManager();

    void addModel(QnRecentLocalConnectionsModel* model);
    void removeModel(QnRecentLocalConnectionsModel* model);

private:
    void updateModelBinding(QnRecentLocalConnectionsModel* model);
    void updateModelsData();

private:
    using QnrecentLocalConnectionsModelPtr = QPointer<QnRecentLocalConnectionsModel>;
    using DataCache = QHash<QnUuid, QnLocalConnectionDataList>;

    QList<QnrecentLocalConnectionsModelPtr> m_models;
    DataCache m_dataCache;
    bool m_updating;
};
