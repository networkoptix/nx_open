
#pragma once

#include <QObject>

#include <base/types.h>
#include <base/requests.h>

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
        /// Getters
        const IntPointer &port() const;

        const StringPointer &password() const;
        
        const StringPointer &systemName() const;

        const DateTimePointer &dateTime() const;

        const ItfUpdateInfoContainerPointer &itfUpdateInfo() const;

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

    private:
        Changeset();

        rtu::ItfUpdateInfo &getItfUpdateInfo(const QString &name);

    private:
        rtu::IntPointer m_port;
        rtu::StringPointer m_password;
        rtu::StringPointer m_systemName;
        rtu::DateTimePointer m_dateTime;
        rtu::ItfUpdateInfoContainerPointer m_itfUpdateInfo;
    };
};