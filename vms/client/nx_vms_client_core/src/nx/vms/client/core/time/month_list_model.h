// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QDate>
#include <QtCore/QLocale>

Q_MOC_INCLUDE("nx/vms/client/core/media/abstract_time_period_storage.h")

namespace nx::vms::client::core {

class AbstractTimePeriodStorage;

/* The model represents 12 months of the year and the archive presence on them. */
class NX_VMS_CLIENT_CORE_API MonthListModel: public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
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
        MonthRole = Qt::UserRole + 1,
        HasArchiveRole,
        AnyCameraHasArchiveRole,
    };

    MonthListModel(QObject* parent = nullptr);
    virtual ~MonthListModel() override;

    int year() const;
    void setYear(int year);

    QLocale locale() const;
    void setLocale(const QLocale& locale);

    AbstractTimePeriodStorage* periodStorage() const;
    void setPeriodStorage(AbstractTimePeriodStorage* store);

    AbstractTimePeriodStorage* allCamerasPeriodStorage() const;
    void setAllCamerasPeriodStorage(AbstractTimePeriodStorage* store);

    qint64 displayOffset() const;
    void setDisplayOffset(qint64 value);

    static void registerQmlType();

public:
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void yearChanged();
    void periodStorageChanged();
    void allCamerasPeriodStorageChanged();
    void localeChanged();
    void displayOffsetChanged();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::core
