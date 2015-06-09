
#pragma once

#include <QObject>

namespace rtu
{
    struct BaseServerInfo;
    struct ExtraServerInfo;
    
    class LoginManager : public QObject
    {
        Q_OBJECT
        
    public:
        LoginManager(QObject *parent = nullptr);
        
        virtual ~LoginManager();
       
        void loginToServer(const BaseServerInfo &info);
        
        void loginToServer(const BaseServerInfo &info
            , const QString &password);
        
    signals:
        void loginOperationFailed(const QUuid &id);
        
        void loginOperationSuccessfull(const QUuid &id
            , const ExtraServerInfo &extraInfo);
        
    private:
        class Impl;
        Impl * const m_impl;        
    };
}
