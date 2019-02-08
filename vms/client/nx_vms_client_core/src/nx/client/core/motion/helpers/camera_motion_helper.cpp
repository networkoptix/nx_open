#include "camera_motion_helper.h"

#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>

namespace nx::vms::client::core {

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
    NX_ASSERT(channel >= 0 && channel < m_motionRegionList.size());

    m_motionRegionList[channel].addRect(sensitivity, rect);
    emit motionRegionListChanged(channel);
}

void CameraMotionHelper::fillRegion(int channel, int sensitivity, const QPoint& at)
{
    NX_ASSERT(sensitivity >= 0 && sensitivity < QnMotionRegion::kSensitivityLevelCount);
    NX_ASSERT(channel >= 0 && channel < m_motionRegionList.size());

    if (m_motionRegionList[channel].updateSensitivityAt(at, sensitivity))
        emit motionRegionListChanged(channel);
}

void CameraMotionHelper::registerQmlType()
{
    qmlRegisterType<CameraMotionHelper>("nx.client.core", 1, 0, "CameraMotionHelper");
}

} // namespace nx::vms::client::core
