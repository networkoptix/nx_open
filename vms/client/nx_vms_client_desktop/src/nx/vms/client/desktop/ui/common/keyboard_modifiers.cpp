// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "keyboard_modifiers.h"

#include <QtQml/QtQml>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/ui/common/keyboard_modifiers_watcher.h>

namespace nx::vms::client::desktop {

struct KeyboardModifiersAttached::Private
{
    Qt::KeyboardModifiers value{KeyboardModifiersWatcher::instance()->modifiers()};
    nx::utils::ScopedConnections connections;
};

// ------------------------------------------------------------------------------------------------
// KeyboardModifiersAttached

KeyboardModifiersAttached::KeyboardModifiersAttached(QObject* parent):
    QObject(parent),
    d(new Private{})
{
    d->connections << connect(
        KeyboardModifiersWatcher::instance(), &KeyboardModifiersWatcher::modifiersChanged, this,
        [this]()
        {
            const auto oldValue = d->value;
            d->value = KeyboardModifiersWatcher::instance()->modifiers();
            emit valueChanged();

            const auto diff = oldValue ^ d->value;
            if (diff.testFlag(Qt::ShiftModifier))
                emit shiftChanged();

            if (diff.testFlag(Qt::ControlModifier))
                emit ctrlChanged();

            if (diff.testFlag(Qt::AltModifier))
                emit altChanged();
        });
}

KeyboardModifiersAttached::~KeyboardModifiersAttached()
{
    // Required here for forward-declared scoped pointer destruction.
}

Qt::KeyboardModifiers KeyboardModifiersAttached::value() const
{
    return d->value;
}

bool KeyboardModifiersAttached::shiftPressed() const
{
    return d->value.testFlag(Qt::ShiftModifier);
}

bool KeyboardModifiersAttached::ctrlPressed() const
{
    return d->value.testFlag(Qt::ControlModifier);
}

bool KeyboardModifiersAttached::altPressed() const
{
    return d->value.testFlag(Qt::AltModifier);
}

// ------------------------------------------------------------------------------------------------
// KeyboardModifiers

KeyboardModifiers::KeyboardModifiers(QObject* parent):
    QObject(parent)
{
}

KeyboardModifiersAttached* KeyboardModifiers::qmlAttachedProperties(QObject* parent)
{
    return new KeyboardModifiersAttached(parent);
}

void KeyboardModifiers::registerQmlType()
{
    qmlRegisterType<KeyboardModifiers>("nx.vms.client.desktop", 1, 0, "KeyboardModifiers");
}

} // namespace nx::vms::client::desktop
