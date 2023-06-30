// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

// Narrowest standard include to use QML_DECLARE_TYPEINFO.
#include <QtQml/QQmlComponent>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class KeyboardModifiersAttached: public QObject
{
    Q_OBJECT
    Q_PROPERTY(Qt::KeyboardModifiers value READ value NOTIFY valueChanged)
    Q_PROPERTY(bool shiftPressed READ shiftPressed NOTIFY shiftChanged)
    Q_PROPERTY(bool ctrlPressed READ ctrlPressed NOTIFY ctrlChanged)
    Q_PROPERTY(bool altPressed READ altPressed NOTIFY altChanged)

public:
    explicit KeyboardModifiersAttached(QObject* parent);
    virtual ~KeyboardModifiersAttached() override;

    Qt::KeyboardModifiers value() const;

    bool shiftPressed() const;
    bool ctrlPressed() const;
    bool altPressed() const;

signals:
    void valueChanged();
    void shiftChanged();
    void ctrlChanged();
    void altChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

class KeyboardModifiers: public QObject
{
    Q_OBJECT

public:
    explicit KeyboardModifiers(QObject* parent = nullptr);

    static KeyboardModifiersAttached* qmlAttachedProperties(QObject* parent);
    static void registerQmlType();
};

} // namespace nx::vms::client::desktop

QML_DECLARE_TYPEINFO(nx::vms::client::desktop::KeyboardModifiers, QML_HAS_ATTACHED_PROPERTIES)
