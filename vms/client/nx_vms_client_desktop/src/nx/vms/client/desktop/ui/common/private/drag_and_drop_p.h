// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMimeData>
#include <QtCore/QObject>
#include <QtCore/QPointer>

Q_MOC_INCLUDE("QtQuick/QQuickItem")

class QQuickItem;

namespace nx::vms::client::desktop {

class DragMimeDataWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* watched READ watched WRITE setWatched NOTIFY watchedChanged)
    Q_PROPERTY(QMimeData* mimeData READ mimeData NOTIFY mimeDataChanged)

public:
    QObject* watched() const;
    void setWatched(QObject* value);

    QMimeData* mimeData() const;

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setMimeData(QMimeData* value);

signals:
    void watchedChanged(QPrivateSignal);
    void mimeDataChanged(QPrivateSignal);

private:
    QPointer<QObject> m_watched;
    QPointer<QMimeData> m_mimeData;
};

class DragHoverWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)

public:
    DragHoverWatcher(QQuickItem* parent = nullptr);

    QQuickItem* target() const;
    void setTarget(QQuickItem* value);

    bool active() const;

signals:
    void started(QPrivateSignal);
    void positionChanged(const QPointF& position, QPrivateSignal);
    void stopped(QPrivateSignal);

    void targetChanged();
    void activeChanged();

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setActive(bool value);

    void handleDrag(const QPoint& globalPos);
    void handleDragStop();

private:
    QPointer<QQuickItem> m_target;
    bool m_active = false;
};

} // namespace nx::vms::client::desktop
