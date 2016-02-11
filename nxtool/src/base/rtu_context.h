
#pragma once

#include <QObject>
#include <QDateTime>
#include <QVariant>

#include <base/types.h>

namespace rtu
{
    /// @class RtuContext
    /// @brief Represents application context which gives access 
    /// to all shared objects and data both from qml and c++ sides.
    /// Also gives access to helper functions
    class RtuContext : public QObject
    {
        Q_OBJECT
        
        Q_PROPERTY(int currentPage READ currentPage NOTIFY currentPageChanged)

        /// Represents current selection object (Selection class instance)
        Q_PROPERTY(QObject* selection READ selection NOTIFY selectionChanged)

        /// Represents current progress task showing on the right panel
        Q_PROPERTY(QObject* progressTask READ progressTask NOTIFY progressTaskChanged)

        Q_PROPERTY(bool isBeta READ isBeta NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolDisplayName READ toolDisplayName NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolVersion READ toolVersion NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolRevision READ toolRevision NOTIFY toolInfoChanged)
        Q_PROPERTY(QString toolSupportLink READ toolSupportLink NOTIFY toolInfoChanged)
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

        /// Hides currently showing on right panel progress task
        void hideProgressTask();

        /// Removes mnimized progress task by its index
        void removeProgressTask(int index);

        /// Called by each progres task after completion
        void applyTaskCompleted(const ApplyChangesTaskPtr &task);

    public slots:
        QObject *selectionModel();

        void tryLoginWith(const QString &primarySystem
            , const QString &password);

    public slots:
        /// Hepler functions section

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

    public:
        bool isBeta() const;

        QString toolDisplayName() const;

        QString toolVersion() const;

        QString toolRevision() const;

        QString toolSupportLink() const;
        
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

