// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtCore/QAbstractListModel>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class TimePeriodsStore;

class NX_VMS_CLIENT_CORE_API CalendarModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

    Q_PROPERTY(int year READ year WRITE setYear
        NOTIFY yearChanged)
    Q_PROPERTY(int month READ month WRITE setMonth
        NOTIFY monthChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale
        NOTIFY localeChanged)
    Q_PROPERTY(nx::vms::client::core::TimePeriodsStore* periodsStore
        READ periodsStore
        WRITE setPeriodsStore
        NOTIFY periodsStoreChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition
        NOTIFY positionChanged)
    Q_PROPERTY(qint64 displayOffset READ displayOffset WRITE setDisplayOffset
        NOTIFY displayOffsetChanged)

public:
    enum Roles
    {
        DayStartTimeRole = Qt::UserRole + 1,
        IsCurrentRole,
        HasArchiveRole
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

    TimePeriodsStore* periodsStore() const;
    void setPeriodsStore(TimePeriodsStore* store);

    qint64 position() const;
    void setPosition(qint64 value);

    qint64 displayOffset() const;
    void setDisplayOffset(qint64 value);

public: // Overrides section.
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

signals:
    void yearChanged();
    void monthChanged();
    void periodsStoreChanged();
    void localeChanged();
    void positionChanged();
    void displayOffsetChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
