
#pragma once

#include <QObject>

#include <base/types.h>

namespace rtu
{
    struct BaseServerInfo;

    class RtuContext;
    class HttpClient;
    class ServersSelectionModel;
    class ApplyChangesTask;

    class ChangesManager : public QObject
    {
        Q_OBJECT

    public:
        ChangesManager(RtuContext *context
            , HttpClient *httpClient
            , ServersSelectionModel *selectionModel
            , QObject *parent);
        
        virtual ~ChangesManager();
        
    public slots:
        QObject *changesProgressModel();

        ApplyChangesTask *notMinimizedTask();

        void removeChangeProgress(QObject *task);

    public slots:
        void addSystemChange(const QString &systemName);
        
        void addPasswordChange(const QString &password);
        
        void addPortChange(int port);
        
        void addDHCPChange(const QString &name
            , bool useDHCP);
        
        void addAddressChange(const QString &name
            , const QString &address);
        
        void addMaskChange(const QString &name
            , const QString &mask);
        
        void addDNSChange(const QString &name
            , const QString &dns);
        
        void addGatewayChange(const QString &name
            , const QString &gateway);
        
        void addDateTimeChange(const QDate &date
            , const QTime &time
            , const QByteArray &timeZoneId);

        void turnOnDhcp();
        
        void applyChanges();
        
        void minimizeProgress();

        void clearChanges();
        
    public slots:
        void serverDiscovered(const rtu::BaseServerInfo &info);
        
        void serversDisappeared(const IDsVector &ids);

        void unknownAdded(const QString &ip);

    private:
        ApplyChangesTaskPtr &changeTask();

    private:
        RtuContext * const m_context;
        HttpClient * const m_httpClient;
        ServersSelectionModel * const m_selectionModel;

        const ChangesProgressModelPtr m_changesModel;
        ApplyChangesTaskPtr m_currentTask;
    };
}
