
#pragma once

#include <QObject>
#include <QDateTime>
#include <QVariant>

#include <base/constants.h>

namespace rtu
{
    class RtuContext : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int currentPage READ currentPage
            WRITE setCurrentPage NOTIFY currentPageChanged)
        Q_PROPERTY(QObject* selection READ selection NOTIFY selectionChanged)

        Q_PROPERTY(QString toolDisplayName READ toolDisplayName NOTIFY toolInfoChanged)

        Q_PROPERTY(bool isBeta READ isBeta NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolVersion READ toolVersion NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolRevision READ toolRevision NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolSupportMail READ toolSupportMail NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolCompanyUrl READ toolCompanyUrl NOTIFY toolInfoChanged)

    public:
        RtuContext(QObject *parent = nullptr);
        
        virtual ~RtuContext();
        
    public:
        void setCurrentPage(int pageId);
        
        int currentPage() const;
        
        ///

        QObject *selection() const;

    public slots:
        QObject *selectionModel();
        
        QObject *ipSettingsModel();
        
        QObject *timeZonesModel(QObject *parent);
        
        QObject *changesManager();
        
        QDateTime applyTimeZone(const QDate &date
            , const QTime &time
            , const QByteArray &prevTimeZoneId
            , const QByteArray &newTimeZoneId);
        
        void tryLoginWith(const QString &primarySystem
            , const QString &password);
        
    public:
        QString toolDisplayName() const;

        bool isBeta() const;

        QString toolVersion() const;

        QString toolRevision() const;

        QString toolSupportMail() const;

        QString toolCompanyUrl() const;

    signals:
        void toolInfoChanged();

        void currentPageChanged();

        void selectionChanged();

        void loginOperationFailed(const QString &primarySystem);
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}

