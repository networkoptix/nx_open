
#pragma once

#include <QObject>

#include <base/types.h>
#include <base/server_info.h>

namespace rtu
{
    class HttpClient;

    class ApplyChangesTask : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool inProgress READ inProgress NOTIFY appliedChangesCountChanged)

        Q_PROPERTY(int totalChangesCount READ totalChangesCount NOTIFY totalChangesCountChanged)
        Q_PROPERTY(int appliedChangesCount READ appliedChangesCount NOTIFY appliedChangesCountChanged)
        Q_PROPERTY(int errorsCount READ errorsCount NOTIFY errorsCountChanged)
        Q_PROPERTY(QObject *successfulModel READ successfulModel NOTIFY changesApplied)
        Q_PROPERTY(QObject *failedModel READ failedModel NOTIFY changesApplied)

    public:
        static ApplyChangesTaskPtr create(const ChangesetPointer &changeset
            , const ServerInfoPtrContainer &target);

        virtual ~ApplyChangesTask();

        IDsVector targetServerIds() const;

    public:
        /// Property getters

        bool inProgress() const;

        int totalChangesCount() const;
        
        int appliedChangesCount() const;

        int errorsCount() const;

        QObject *successfulModel();
        
        QObject *failedModel();

    public:
        /// Discovery events

        void serverDiscovered(const rtu::BaseServerInfo &info);

        void serversDisappeared(const IDsVector &ids);

        void unknownAdded(const QString &ip);

    signals: 
        void totalChangesCountChanged();
        
        void appliedChangesCountChanged();

        void errorsCountChanged();

        void itfUpdated(const QUuid &id
            , const QString &hostAddress
            , const InterfaceInfoList &itf);

        void dateTimeUpdated(const QUuid &id
            , qint64 utcMsecs
            , const QByteArray &timeZone
            , qint64 timestampMs);

        void systemNameUpdated(const QUuid &id
            , const QString &systemName);

        void portUpdated(const QUuid &id
            , int port);

        void passwordUpdated(const QUuid &id
            , const QString &password);

        void changesApplied();

    private:
        ApplyChangesTask(const ChangesetPointer &changeset
            , const ServerInfoPtrContainer &targets);

    private:
        class Impl;
        typedef std::shared_ptr<Impl> ImplHolder;   

        const ImplHolder m_impl;
    };
}