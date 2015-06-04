
#pragma once

#include <QUuid>
#include <QVector>
#include <QSharedPointer>

namespace rtu
{
    class BaseServerInfo;
    typedef QVector<QUuid> IDsVector;
    
    class ServersFinder : public QObject
    {
        Q_OBJECT
        
    public:
        typedef QSharedPointer<ServersFinder> Holder;
        
        static Holder create();
        
        virtual ~ServersFinder();

    signals:
        void onAddedServer(const BaseServerInfo &baseInfo);
        void onChangedServer(const BaseServerInfo &baseInfo);
        void onRemovedServers(const IDsVector &removed);

    private:
        ServersFinder();
        
    private:
        class Impl;
        
        Impl * const m_impl;
    };
}
