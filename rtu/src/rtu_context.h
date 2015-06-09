
#pragma once

#include <QObject>
#include <QDateTime>
#include <QVariant>


namespace rtu
{
    class HttpClient;
    
    class RtuContext : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int selectedServersCount READ selectedServersCount NOTIFY selectionChagned)
        Q_PROPERTY(int selectionPort READ selectionPort NOTIFY selectionChagned)
        Q_PROPERTY(QString selectionSystemName READ selectionSystemName NOTIFY selectionChagned)
        Q_PROPERTY(QString selectionPassword READ selectionPassword NOTIFY selectionChagned)
        Q_PROPERTY(bool allowEditIpAddresses READ allowEditIpAddresses NOTIFY selectionChagned)

        Q_PROPERTY(QDateTime selectionDateTime READ selectionDateTime NOTIFY selectionChagned)
        
        Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
                
    public:
        enum Pages
        {
            kSettingsPage
            , kProgressPage
            , kSummaryPage
        };
        Q_ENUMS(Pages)
        
    public:
        RtuContext(QObject *parent = nullptr);
        
        virtual ~RtuContext();
        
    public slots:
        QObject *selectionModel();
        
        QObject *ipSettingsModel();
        
        QObject *timeZonesModel(QObject *parent);
        
        QObject *changesManager();
        
        HttpClient *httpClient();
        
        QDateTime applyTimeZone(const QDate &date
            , const QTime &time
            , const QByteArray &prevTimeZoneId
            , const QByteArray &newTimeZoneId);
        
    public slots:
        int selectedServersCount() const;
        
        int selectionPort() const;
        
        QDateTime selectionDateTime() const;
        
        QString selectionSystemName() const;
        
        QString selectionPassword() const;
        
        bool allowEditIpAddresses() const;
        
        void tryLoginWith(const QString &password);
        
    public:
        void setCurrentPage(int pageId);
        
        int currentPage() const;
        
    signals:
        void currentPageChanged();
        
        void selectionChagned();
        
    private:
        class Impl;
        
        Impl * const m_impl;
    };
}

