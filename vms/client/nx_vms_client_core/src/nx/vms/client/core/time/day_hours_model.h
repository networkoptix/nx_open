// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("nx/vms/client/core/media/abstract_time_period_storage.h")

namespace nx::vms::client::core {

class AbstractTimePeriodStorage;

/* The model represents 24 day hours and the archive presence on that hours. */
class NX_VMS_CLIENT_CORE_API DayHoursModel: public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool amPmTime READ amPmTime WRITE setAmPmTime NOTIFY amPmTimeChanged)
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
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
    Q_PROPERTY(QTimeZone timeZone READ timeZone WRITE setTimeZone
        NOTIFY timeZoneChanged)

public:
    enum Role
    {
        HourRole = Qt::UserRole + 1,
        HasArchiveRole,
        AnyCameraHasArchiveRole,
        IsHourValidRole,
    };

    DayHoursModel(QObject* parent = nullptr);
    virtual ~DayHoursModel() override;

    bool amPmTime() const;
    void setAmPmTime(bool amPmTime);

    QDate date() const;
    void setDate(const QDate& date);

    AbstractTimePeriodStorage* periodStorage() const;
    void setPeriodStorage(AbstractTimePeriodStorage* store);

    AbstractTimePeriodStorage* allCamerasPeriodStorage() const;
    void setAllCamerasPeriodStorage(AbstractTimePeriodStorage* store);

    qint64 displayOffset() const;
    void setDisplayOffset(qint64 value);

    QTimeZone timeZone() const;
    void setTimeZone(QTimeZone value);

    static void registerQmlType();

public:
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void amPmTimeChanged();
    void dateChanged();
    void periodStorageChanged();
    void allCamerasPeriodStorageChanged();
    void displayOffsetChanged();
    void timeZoneChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
