// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQmlIntegration/QtQmlIntegration>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class SessionAwareCloseEvent: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool force MEMBER force CONSTANT)
    Q_PROPERTY(bool accepted MEMBER accepted)

public:
    bool force = false;
    bool accepted = true;
};

class SessionAwareAttached: public QObject
{
    Q_OBJECT

public:
    explicit SessionAwareAttached(QObject* parent);
    virtual ~SessionAwareAttached() override;

signals:
    void tryClose(SessionAwareCloseEvent* event);
    void forcedUpdate();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * Allows to use QnSessionAwareDelegate callbacks inside QML as attached properties.
 */
class SessionAware: public QObject
{
    Q_OBJECT
    QML_ATTACHED(SessionAwareAttached)

public:
    static SessionAwareAttached* qmlAttachedProperties(QObject* object);

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
