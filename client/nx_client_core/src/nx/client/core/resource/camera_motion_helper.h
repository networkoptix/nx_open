#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <core/resource/motion_window.h>

namespace nx {
namespace client {
namespace core {

class CameraMotionHelper: public QObject
{
    Q_OBJECT

public:
    CameraMotionHelper(QObject* parent = nullptr);

    QList<QnMotionRegion> motionRegionList() const;
    void setMotionRegionList(const QList<QnMotionRegion>& value);

    void restoreDefaults();

    void addRect(int channel, int sensitivity, const QRect& rect);

    static void registerQmlType();

signals:
    void cameraChanged();
    void motionRegionListChanged(int channel = -1/*all*/);

private:
    QList<QnMotionRegion> m_motionRegionList;
};

} // namespace core
} // namespace client
} // namespace nx
