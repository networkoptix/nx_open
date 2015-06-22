
#pragma once

#include <QObject>

class QString;

namespace rtu
{
    struct BaseServerInfo;

    class RtuContext;
    class HttpClient;
    class ServersSelectionModel;
    
    class ChangesManager : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int totalChangesCount READ totalChangesCount NOTIFY totalChangesCountChanged)
        Q_PROPERTY(int appliedChangesCount READ appliedChangesCount NOTIFY appliedChangesCountChanged)

    public:
        ChangesManager(RtuContext *context
            , HttpClient *httpClient
            , ServersSelectionModel *selectionModel
            , QObject *parent);
        
        virtual ~ChangesManager();
        
    public slots:
        QObject *successfulResultsModel();
        
        QObject *failedResultsModel();
        
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
        
        void clearChanges();
        
    public slots:
        int totalChangesCount() const;
        
        int appliedChangesCount() const;
        
        void serverDiscovered(const rtu::BaseServerInfo &info);
        
    signals:
        void totalChangesCountChanged();
        
        void appliedChangesCountChanged();
        
    private:
        class Impl;
        
        Impl * const m_impl;
    };
}
