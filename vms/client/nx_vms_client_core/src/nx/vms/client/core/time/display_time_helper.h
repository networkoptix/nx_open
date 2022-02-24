// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QLocale>
#include <QtCore/QDateTime>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

class DisplayTimeHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(qint64 position READ position WRITE setPosition
        NOTIFY positionChanged)
    Q_PROPERTY(qint64 displayOffset READ displayOffset WRITE setDisplayOffset
        NOTIFY displayOffsetChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale
        NOTIFY localeChanged)
    Q_PROPERTY(bool is24HoursTimeFormat READ is24HoursTimeFormat WRITE set24HoursTimeFormat
        NOTIFY is24HoursTimeFormatChanged)

    Q_PROPERTY(QString fullDate READ fullDate NOTIFY dateTimeChanged)
    Q_PROPERTY(QString hours READ hours NOTIFY dateTimeChanged)
    Q_PROPERTY(QString minutes READ minutes NOTIFY dateTimeChanged)
    Q_PROPERTY(QString seconds READ seconds NOTIFY dateTimeChanged)
    Q_PROPERTY(QString noonMark READ noonMark NOTIFY dateTimeChanged)

public:
    static void registerQmlType();

    DisplayTimeHelper(QObject* parent = nullptr);
    virtual ~DisplayTimeHelper() override;

    void setPosition(qint64 value);
    qint64 position() const;

    void setDisplayOffset(qint64 value);
    qint64 displayOffset() const;

    void setLocale(const QLocale& locale);
    QLocale locale() const;

    void set24HoursTimeFormat(bool value);
    bool is24HoursTimeFormat() const;

    QString fullDate() const;
    QString hours() const;
    QString minutes() const;
    QString seconds() const;
    QString noonMark() const;

signals:
    void positionChanged();
    void displayOffsetChanged();
    void localeChanged();
    void dateTimeChanged();
    void is24HoursTimeFormatChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
