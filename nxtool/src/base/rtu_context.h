
#pragma once

#include <QObject>
#include <QDateTime>
#include <QVariant>

#include <base/types.h>
#include <base/constants.h>

namespace rtu
{
    class ApplyChangesTask;

    class RtuContext : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int currentPage READ currentPage NOTIFY currentPageChanged)
        Q_PROPERTY(QObject* selection READ selection NOTIFY selectionChanged)
        Q_PROPERTY(QObject* progressTask READ progressTask NOTIFY progressTaskChanged)
        Q_PROPERTY(QString toolDisplayName READ toolDisplayName NOTIFY toolInfoChanged)

        Q_PROPERTY(bool isBeta READ isBeta NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolVersion READ toolVersion NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolRevision READ toolRevision NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolSupportMail READ toolSupportMail NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolCompanyUrl READ toolCompanyUrl NOTIFY toolInfoChanged)

        Q_PROPERTY(bool showWarnings READ showWarnings WRITE setShowWarnings NOTIFY showWarningsChanged)

    public:
        RtuContext(QObject *parent = nullptr);
        
        virtual ~RtuContext();
        
    public:
        int currentPage() const;
        
        bool showWarnings() const;

        void setShowWarnings(bool show);

        ///

        QObject *selection() const;

    public slots:
        /// Changes management

        QObject *changesManager();

        QObject *progressTask();

        void showProgressTask(const ApplyChangesTaskPtr &task);

        void showProgressTaskFromList(int index);

        void hideProgressTask();

        void removeProgressTask(int index);

        void applyTaskCompleted(const ApplyChangesTaskPtr &task);

    public slots:
        QObject *selectionModel();
        
        QObject *ipSettingsModel();
        
        QObject *timeZonesModel(QObject *parent);
        
        bool isValidSubnetMask(const QString &mask) const;

        bool isDiscoverableFromCurrentNetwork(const QString &ip
            , const QString &mask) const;

        bool isDiscoverableFromNetwork(const QString &ip
            , const QString &mask
            , const QString &subnet
            , const QString &subnetMask) const;

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

        void showWarningsChanged();

        void loginOperationFailed(const QString &primarySystem);

        void progressTaskChanged();
        
    private:
        class Impl;
        Impl * const m_impl;
    };
}

