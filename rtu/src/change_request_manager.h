
#pragma once

#include <QObject>

class QString;

namespace rtu
{
    class RtuContext;
    class ServersSelectionModel;
    
    class ChangeRequestManager : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int totalChangesCount READ totalChangesCount NOTIFY totalChangesCountChanged)
        Q_PROPERTY(int appliedChangesCount READ appliedChangesCount NOTIFY appliedChangesCountChanged)

        Q_PROPERTY(int errorsCount READ errorsCount NOTIFY errorsCountChanged)

    public:
        ChangeRequestManager(RtuContext *context
            , ServersSelectionModel *selectionModel
            , QObject *parent);
        
        virtual ~ChangeRequestManager();
        
    public slots:
        QObject *successfulResultsModel();
        
        QObject *failedResultsModel();
        
        void addSystemChangeRequest(const QString &newSystemName);
        
        void addPasswordChangeRequest(const QString &password);
        
        void addPortChangeRequest(int port);
        
        void addIpChangeRequest(const QString &name
            , bool useDHCP                                
            , const QString &address
            , const QString &subnetMask);
        
        void addSelectionDHCPStateChange(bool useDHCP);
        
        void addDateTimeChange(const QDate &date
            , const QTime &time
            , const QByteArray &timeZoneId);
        
        void clearChanges();

        void applyChanges();
        
    public slots:
        int totalChangesCount() const;
        
        int appliedChangesCount() const;
        
        int errorsCount() const;
        
    signals:
        void totalChangesCountChanged();
        
        void appliedChangesCountChanged();
        
        void errorsCountChanged();
        
    private:
        class Impl;
        
        Impl * const m_impl;
    };
}
