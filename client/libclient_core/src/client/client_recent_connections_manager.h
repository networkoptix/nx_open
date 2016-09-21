#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/utils/singleton.h>

#include <client_core/local_connection_data.h>

class QnrecentLocalConnectionsModel;

class QnClientRecentConnectionsManager: public QObject, public Singleton<QnClientRecentConnectionsManager>
{
    typedef QObject base_type;
    Q_OBJECT

public:
    QnClientRecentConnectionsManager();
    virtual ~QnClientRecentConnectionsManager();

    void addModel(QnrecentLocalConnectionsModel* model);
    void removeModel(QnrecentLocalConnectionsModel* model);

private:
    void updateModelBinding(QnrecentLocalConnectionsModel* model);
    void updateModelsData();

private:
    using QnrecentLocalConnectionsModelPtr = QPointer<QnrecentLocalConnectionsModel>;
    using DataCache = QHash<QString, QnLocalConnectionDataList>;

    QList<QnrecentLocalConnectionsModelPtr> m_models;
    DataCache m_dataCache;
    bool m_updating;
};
