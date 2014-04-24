#ifndef __QN_ABSTRACT_CAMERA_DATA_H__
#define __QN_ABSTRACT_CAMERA_DATA_H__

#include <common/common_globals.h>

//#include <recording/time_period_list.h>

class QnTimePeriodList;

class QnAbstractCameraData;
typedef QSharedPointer<QnAbstractCameraData> QnAbstractCameraDataPtr;

class QnAbstractCameraData {
public:
    //QnAbstractCameraData(): m_dataType(Qn::CameraDataTypeCount) {}
    QnAbstractCameraData(const Qn::CameraDataType dataType);
    
    virtual void append(const QnAbstractCameraDataPtr &other);

    virtual QnAbstractCameraDataPtr merge(const QVector<QnAbstractCameraDataPtr> &source);

    virtual QnTimePeriodList dataSource() const;
    bool isEmpty() const;

    virtual void clear();

    virtual bool trimDataSource(qint64 trimTime);

    virtual bool operator==(const QnAbstractCameraDataPtr &other) const;

    Qn::CameraDataType dataType() const;
protected:
    const Qn::CameraDataType m_dataType;
};

Q_DECLARE_METATYPE(QnAbstractCameraDataPtr);

Qn::CameraDataType timePeriodToDataType(const Qn::TimePeriodContent timePeriodType);
Qn::TimePeriodContent dataTypeToPeriod(const Qn::CameraDataType dataType);

#endif // __QN_ABSTRACT_CAMERA_DATA_H__