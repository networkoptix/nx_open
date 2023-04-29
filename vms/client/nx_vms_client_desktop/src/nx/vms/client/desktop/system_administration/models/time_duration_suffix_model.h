// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtQml/QQmlEngine>

namespace nx::vms::client::desktop {

/**
 * A model for displaying time duration suffixes: seconds, minutes, hours.
 */
class TimeDurationSuffixModel: public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

    using base_type = QAbstractListModel;

    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

public:
    enum class Suffix
    {
        secondsIndex = 0,
        minutesIndex,
        hoursIndex,

        count
    };
    Q_ENUM(Suffix)

public:
    explicit TimeDurationSuffixModel();
    virtual ~TimeDurationSuffixModel() override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    int value() const { return m_value; }
    void setValue(int value);

    Q_INVOKABLE static int multiplierAt(int row);

    static void registerQmlType();

signals:
    void valueChanged();

private:
    int m_value = 0;
};

} // namespace nx::vms::client::desktop
