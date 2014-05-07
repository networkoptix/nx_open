#ifndef __QN_ABSTRACT_CAMERA_DATA_H__
#define __QN_ABSTRACT_CAMERA_DATA_H__

#include <common/common_globals.h>

#include <camera/data/camera_data_fwd.h>

class QnTimePeriodList;

class QnAbstractCameraData {
public:
    QnAbstractCameraData();
    
    virtual void append(const QnAbstractCameraDataPtr &other);

    virtual QnAbstractCameraDataPtr merge(const QVector<QnAbstractCameraDataPtr> &source);

    virtual QnTimePeriodList dataSource() const;
    virtual bool isEmpty() const;

    virtual void clear();

    virtual bool contains(const QnAbstractCameraDataPtr & data) const;
};

Q_DECLARE_METATYPE(QnAbstractCameraDataPtr);

Qn::CameraDataType timePeriodToDataType(const Qn::TimePeriodContent timePeriodType);
Qn::TimePeriodContent dataTypeToPeriod(const Qn::CameraDataType dataType);

#endif // __QN_ABSTRACT_CAMERA_DATA_H__