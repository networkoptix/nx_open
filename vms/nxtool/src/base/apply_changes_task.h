
#pragma once

#include <QObject>

#include <base/types.h>
#include <nx/vms/server/api/server_info.h>

namespace rtu
{
    class HttpClient;

    /// @class ApplyChangesTask
    /// @brief Applies specified changes to selected servers and contains information about its progress
    /// Also it is indirect source for data updates in ServersSelectionModel instance
    /// Instance is created by ChangesManager and lives at least until all specified changes are applied or failed.
    class ApplyChangesTask : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool inProgress READ inProgress NOTIFY appliedChangesCountChanged)

        Q_PROPERTY(QString progressPageHeaderText READ progressPageHeaderText NOTIFY neverCalledSignal)
        Q_PROPERTY(QString minimizedText READ minimizedText NOTIFY neverCalledSignal)
        
        Q_PROPERTY(int totalChangesCount READ totalChangesCount NOTIFY totalChangesCountChanged)
        Q_PROPERTY(int appliedChangesCount READ appliedChangesCount NOTIFY appliedChangesCountChanged)
        Q_PROPERTY(int errorsCount READ errorsCount NOTIFY errorsCountChanged)
        Q_PROPERTY(QObject *successfulModel READ successfulModel NOTIFY changesApplied)
        Q_PROPERTY(QObject *failedModel READ failedModel NOTIFY changesApplied)

    public:
        typedef nx::vms::server::api::BaseServerInfo BaseServerInfo;
        typedef nx::vms::server::api::ExtraServerInfo ExtraServerInfo;
        typedef nx::vms::server::api::ServerInfoPtrContainer ServerInfoPtrContainer;

        static ApplyChangesTaskPtr create(const ChangesetPointer &changeset
            , const ServerInfoPtrContainer &target);

        virtual ~ApplyChangesTask();

        IDsVector targetServerIds() const;

        const QUuid &id() const;

    public:
        /// Property getters

        QString progressPageHeaderText() const;

        QString minimizedText() const;

        bool inProgress() const;

        int totalChangesCount() const;
        
        int appliedChangesCount() const;

        int errorsCount() const;

        QObject *successfulModel();
        
        QObject *failedModel();

    public:
        /// Discovery events

        void serverDiscovered(const BaseServerInfo &info);

        void serversDisappeared(const IDsVector &ids);

        void unknownAdded(const QString &ip);

    signals: 
        void neverCalledSignal();

        void totalChangesCountChanged();
        
        void appliedChangesCountChanged();

        void errorsCountChanged();

        void extraInfoUpdated(const QUuid &id
            , const ExtraServerInfo &extraInfo
            , const QString &hostAddress);

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