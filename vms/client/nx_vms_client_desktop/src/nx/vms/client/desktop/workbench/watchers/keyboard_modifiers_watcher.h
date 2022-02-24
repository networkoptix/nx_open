// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QtGlobal>

namespace nx::vms::client::desktop {

/**
 * An application-wide notifier of keyboard modifier changes.
 */
class KeyboardModifiersWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit KeyboardModifiersWatcher(QObject* parent = nullptr);

    Qt::KeyboardModifiers modifiers() const;

signals:
    void modifiersChanged(Qt::KeyboardModifiers changedModifiers);

private:
    Qt::KeyboardModifiers m_modifiers{};
};

} // namespace nx::vms::client::desktop
