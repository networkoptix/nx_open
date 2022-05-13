// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <core/resource/motion_window.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CameraMotionHelper: public QObject
{
    Q_OBJECT

public:
    CameraMotionHelper(QObject* parent = nullptr);

    QList<QnMotionRegion> motionRegionList() const;
    void setMotionRegionList(const QList<QnMotionRegion>& value);

    Q_INVOKABLE void restoreDefaults();
    Q_INVOKABLE void addRect(int channel, int sensitivity, const QRect& rect);
    Q_INVOKABLE void fillRegion(int channel, int sensitivity, const QPoint& at);

    static void registerQmlType();

signals:
    void motionRegionListChanged(int channel = -1/*all*/);

private:
    QList<QnMotionRegion> m_motionRegionList;
};

} // namespace nx::vms::client::core
