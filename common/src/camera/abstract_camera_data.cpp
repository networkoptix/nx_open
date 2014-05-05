#include "abstract_camera_data.h"

#include <recording/time_period_list.h>

QnAbstractCameraData::QnAbstractCameraData(const Qn::CameraDataType dataType):
    m_dataType(dataType)
{
}

bool QnAbstractCameraData::isEmpty() const { 
    return dataSource().isEmpty(); 
};

void QnAbstractCameraData::append(const QnAbstractCameraDataPtr &other) {
    Q_UNUSED(other);
};

QnAbstractCameraDataPtr QnAbstractCameraData::merge(const QVector<QnAbstractCameraDataPtr> &source) {
    Q_UNUSED(source);
    return QnAbstractCameraDataPtr();
}

Qn::CameraDataType QnAbstractCameraData::dataType() const {
    return m_dataType;
}

QnTimePeriodList QnAbstractCameraData::dataSource() const {
    return QnTimePeriodList();
}

void QnAbstractCameraData::clear() {

}

bool QnAbstractCameraData::trimDataSource(qint64 trimTime) {
    Q_UNUSED(trimTime)
    return false;
}

bool QnAbstractCameraData::contains(const QnAbstractCameraDataPtr & data) const {
    Q_UNUSED(data)
    return false;
}

Qn::CameraDataType timePeriodToDataType(const Qn::TimePeriodContent timePeriodType) {
    switch (timePeriodType) {
    case Qn::RecordingContent:
        return Qn::RecordedTimePeriod;
    case Qn::MotionContent:
        return Qn::MotionTimePeriod;
    case Qn::BookmarksContent:
        return Qn::BookmarkTimePeriod;
    default:
        assert(false); //should never get here
        break;
    }
    return Qn::CameraDataTypeCount;
}

Qn::TimePeriodContent dataTypeToPeriod(const Qn::CameraDataType dataType) {
    switch (dataType)
    {
    case Qn::RecordedTimePeriod:
        return Qn::RecordingContent;
    case Qn::MotionTimePeriod:
        return Qn::MotionContent;
    case Qn::BookmarkTimePeriod:
        return Qn::BookmarksContent;
    default:
        assert(false); //should never get here
        break;
    }
    return Qn::TimePeriodContentCount;
}