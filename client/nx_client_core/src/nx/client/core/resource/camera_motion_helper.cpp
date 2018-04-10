#include "camera_motion_helper.h"

#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>

namespace nx {
namespace client {
namespace core {

CameraMotionHelper::CameraMotionHelper(QObject* parent):
    QObject(parent)
{
}

QList<QnMotionRegion> CameraMotionHelper::motionRegionList() const
{
    return m_motionRegionList;
}

void CameraMotionHelper::setMotionRegionList(const QList<QnMotionRegion>& value)
{
    if (m_motionRegionList == value)
        return;

    m_motionRegionList = value;
    emit motionRegionListChanged();
}

void CameraMotionHelper::restoreDefaults()
{
    bool changed = false;
    static const QnMotionRegion defaultRegion;

    for (auto& regions: m_motionRegionList)
    {
        if (regions == defaultRegion)
            continue;

        regions = defaultRegion;
        changed = true;
    }

    if (changed)
        emit motionRegionListChanged();
}

void CameraMotionHelper::addRect(int channel, int sensitivity, const QRect& rect)
{
    NX_ASSERT(sensitivity >= 0 && sensitivity < QnMotionRegion::kSensitivityLevelCount);

    if (channel >= m_motionRegionList.size())
        return; //< TODO: #vkutin Error or not?

    m_motionRegionList[channel].addRect(sensitivity, rect);
    emit motionRegionListChanged(channel);
}

void CameraMotionHelper::registerQmlType()
{
    qmlRegisterType<CameraMotionHelper>("nx.client.core", 1, 0, "CameraMotionHelper");
}

} // namespace core
} // namespace client
} // namespace nx
