// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("nx/vms/client/core/media/time_period_storage.h")

namespace nx::vms::client::core {

class AbstractTimePeriodStorage;

class NX_VMS_CLIENT_CORE_API CalendarModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY(int month READ month WRITE setMonth NOTIFY monthChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QTimeZone timeZone READ timeZone WRITE setTimeZone NOTIFY timeZoneChanged)
    Q_PROPERTY(nx::vms::client::core::AbstractTimePeriodStorage* periodStorage
        READ periodStorage
        WRITE setPeriodStorage
        NOTIFY periodStorageChanged)
    Q_PROPERTY(nx::vms::client::core::AbstractTimePeriodStorage* allCamerasPeriodStorage
        READ allCamerasPeriodStorage
        WRITE setAllCamerasPeriodStorage
        NOTIFY allCamerasPeriodStorageChanged)
    Q_PROPERTY(qint64 displayOffset READ displayOffset WRITE setDisplayOffset
        NOTIFY displayOffsetChanged)

public:
    enum Role
    {
        DateRole = Qt::UserRole + 1,
        DayStartTimeRole,
        HasArchiveRole,
        AnyCameraHasArchiveRole,
    };

    static void registerQmlType();

    CalendarModel(QObject* parent = nullptr);
    virtual ~CalendarModel() override;

public: // Properties and invokables.
    int year() const;
    void setYear(int year);

    int month() const;
    void setMonth(int month);

    QLocale locale() const;
    void setLocale(const QLocale& locale);

    QTimeZone timeZone() const;
    void setTimeZone(const QTimeZone& timeZone);

    AbstractTimePeriodStorage* periodStorage() const;
    void setPeriodStorage(AbstractTimePeriodStorage* store);

    AbstractTimePeriodStorage* allCamerasPeriodStorage() const;
    void setAllCamerasPeriodStorage(AbstractTimePeriodStorage* store);

    qint64 displayOffset() const;
    void setDisplayOffset(qint64 value);

public: // Overrides section.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void yearChanged();
    void monthChanged();
    void periodStorageChanged();
    void allCamerasPeriodStorageChanged();
    void localeChanged();
    void timeZoneChanged();
    void displayOffsetChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
