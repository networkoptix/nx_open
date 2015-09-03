
#pragma once

#include <QObject>

#include <base/requests.h>

namespace rtu
{
    class RtuContext;
    class ServersSelectionModel;
    class ChangesSummaryModel;
    class ChangesManager;

    class ApplyChangesTask : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool inProgress READ inProgress NOTIFY appliedChangesCountChanged)
        Q_PROPERTY(int totalChangesCount READ totalChangesCount NOTIFY totalChangesCountChanged)
        Q_PROPERTY(int appliedChangesCount READ appliedChangesCount NOTIFY appliedChangesCountChanged)

    public:
        ApplyChangesTask(rtu::RtuContext *context
            , rtu::HttpClient *httpClient
            , rtu::ServersSelectionModel *selectionModel
            , ChangesManager *changesManager);

        virtual ~ApplyChangesTask();

        void setAutoremoveOnComplete();

        ///

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

        IDsVector targetServerIds() const;

    public slots:  

        QObject *successfulResultsModel();
        
        QObject *failedResultsModel();

        /// Properties' getters

        bool inProgress() const;

        int totalChangesCount() const;
        
        int appliedChangesCount() const;

        /// Discovery events

        void serverDiscovered(const rtu::BaseServerInfo &info);

        void serversDisappeared(const IDsVector &ids);

        bool unknownAdded(const QString &ip);

    signals: 
        void totalChangesCountChanged();
        
        void appliedChangesCountChanged();

        void changesApplied();

    private:
        ItfUpdateInfo &getItfUpdateInfo(const QString &name);

        void onChangesetApplied(int changesCount);

        void sendRequests();

        void addDateTimeChangeRequests();

        void addSystemNameChangeRequests();
    
        void addPortChangeRequests();
    
        void addIpChangeRequests();

        void addPasswordChangeRequests();    

        bool addSummaryItem(const rtu::ServerInfo &info
            , const QString &description
            , const QString &value
            , const QString &error
            , AffectedEntities checkFlags
            , AffectedEntities affected);

        bool addUpdateInfoSummaryItem(const rtu::ServerInfo &info
            , const QString &error
            , const ItfUpdateInfo &change
            , AffectedEntities affected);

    private:
        struct ItfChangeRequest;

        typedef QHash<QUuid, ItfChangeRequest> ItfChangesContainer;
        void itfRequestSuccessfullyApplied(const ItfChangesContainer::iterator it);

    private:
        struct Changeset;
        typedef std::unique_ptr<Changeset> ChangesetHolder;
        typedef QVector<rtu::ServerInfo> ServerInfoValueContainer;
    
        typedef std::function<void ()> Request;
        typedef QPair<bool, Request> RequestData;
        typedef QVector<RequestData>  RequestContainer;

        const ChangesetHolder m_changeset;
        ChangesManager * const m_changesManager;
        RtuContext * const m_context;
        HttpClient * const m_client;
        ServersSelectionModel * const m_model;

        ChangesSummaryModel *m_succesfulChangesModel;
        ChangesSummaryModel *m_failedChangesModel;
    
        ServerInfoValueContainer m_serverValues;
        ServerInfoPtrContainer m_targetServers;
        IDsVector m_lockedItems;
        RequestContainer m_requests;
        ItfChangesContainer m_itfChanges;

        int m_appliedChangesIndex;
        int m_lastChangesetIndex;

        int m_totalChangesCount;
        int m_appliedChangesCount;
        bool m_autoremoveOnComplete;
    };
}