// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/core/ui/frame_section.h>

namespace nx::vms::client::desktop {

class CursorManager: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* target READ target WRITE setTarget NOTIFY targetChanged)

public:
    CursorManager(QObject* parent = nullptr);
    virtual ~CursorManager() override;

    QObject* target() const;
    void setTarget(QObject* target);

    Q_INVOKABLE void setCursor(QObject* requester, Qt::CursorShape shape, int rotation);
    // TODO: Use FrameSection::Section once QTBUG-58454 is fixed.
    Q_INVOKABLE void setCursorForFrameSection(
        QObject* requester, int section, int rotation = 0);
    Q_INVOKABLE void setCursorForFrameSection(
        int section, int rotation = 0);
    Q_INVOKABLE void unsetCursor(QObject* requester = nullptr);

    Q_INVOKABLE QPoint pos() const;

    static void registerQmlType();

signals:
    void targetChanged();

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::desktop
