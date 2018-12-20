
#pragma once

#include <QObject>

#include <base/types.h>
#include <nx/vms/server/api/client.h>
#include <helpers/time_helper.h>

namespace rtu
{
    class Changeset : public QObject
    {
        Q_OBJECT
    public:
        /// Creates shared pointer to chagneset with helpers::qml_objects_parent() parent
        static ChangesetPointer create();

        ~Changeset();

    public:
        typedef nx::vms::server::api::Client Client;

        /// Getters
        const IntPointer &port() const;

        const StringPointer &password() const;

        const StringPointer &systemName() const;

        const DateTimePointer &dateTime() const;

        const Client::ItfUpdateInfoContainerPtr &itfUpdateInfo() const;

        ///

        bool softRestart() const;

        bool osRestart() const;

        bool factoryDefaults() const;

        bool factoryDefaultsButNetwork() const;

        QString getProgressText() const;

        QString getMinimizedProgressText() const;

    public slots:
        /// Special setters available from Qml
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

        void addSoftRestartAction();

        void addOsRestartAction();

        void addFactoryDefaultsAction();

        void addFactoryDefaultsButNetworkAction();

    private:
        Changeset();

        void preapareActionData();

        void preparePropChangesData();

        Client::ItfUpdateInfo &getItfUpdateInfo(const QString &name);

    private:
        IntPointer m_port;
        StringPointer m_password;
        StringPointer m_systemName;
        DateTimePointer m_dateTime;
        Client::ItfUpdateInfoContainerPtr m_itfUpdateInfo;

        bool m_softRestart;
        bool m_osRestart;
        bool m_factoryDefaults;
        bool m_factoryDefaultsButNetwork;
    };
};