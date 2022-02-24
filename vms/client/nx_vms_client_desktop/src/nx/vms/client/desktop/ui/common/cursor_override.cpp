// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cursor_override.h"

#include <QtCore/QList>

#include <QtGui/QGuiApplication>
#include <QtGui/QCursor>

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

struct CursorOverrideAttached::Private
{
    Qt::CursorShape shape = Qt::ArrowCursor;
    bool active = false;
    static QList<QPointer<CursorOverrideAttached>> cursors;
};

QList<QPointer<CursorOverrideAttached>> CursorOverrideAttached::Private::cursors;

CursorOverrideAttached::CursorOverrideAttached(QObject* parent):
    QObject(parent),
    d(new Private())
{
}

CursorOverrideAttached::~CursorOverrideAttached()
{
    setActive(false);
}

Qt::CursorShape CursorOverrideAttached::shape() const
{
    return d->shape;
}

void CursorOverrideAttached::setShape(Qt::CursorShape value)
{
    if (d->shape == value)
        return;

    d->shape = value;

    if (effectivelyActive())
        QGuiApplication::changeOverrideCursor(d->shape);

    emit shapeChanged({});
}

bool CursorOverrideAttached::active() const
{
    return d->active;
}

void CursorOverrideAttached::setActive(bool value)
{
    if (d->active == value)
        return;

    const bool wasCurrent = effectivelyActive();
    d->active = value;

    if (value)
    {
        Private::cursors.push_back(this);
        QGuiApplication::setOverrideCursor(d->shape);
    }
    else if (wasCurrent)
    {
        while (!Private::cursors.empty())
        {
            const auto cursor = Private::cursors.back();
            if (cursor && cursor->active())
                break;

            QGuiApplication::restoreOverrideCursor();
            Private::cursors.pop_back();
        }
    }

    emit activeChanged({});
}

bool CursorOverrideAttached::effectivelyActive() const
{
    return d->active && NX_ASSERT(!Private::cursors.empty()) && Private::cursors.back() == this;
}

CursorOverrideAttached* CursorOverride::qmlAttachedProperties(QObject* parent)
{
    return new CursorOverrideAttached(parent);
}

void CursorOverride::registerQmlType()
{
    qmlRegisterType<CursorOverride>("nx.vms.client.desktop", 1, 0, "CursorOverride");
}

} // namespace nx::vms::client::desktop
