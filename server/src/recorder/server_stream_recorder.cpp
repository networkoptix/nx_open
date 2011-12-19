#include "server_stream_recorder.h"
#include "motion/motion_helper.h"

QnServerStreamRecorder::QnServerStreamRecorder(QnResourcePtr dev):
    QnStreamRecorder(dev)
{
    m_skipDataToTime = AV_NOPTS_VALUE;
    m_lastMotionTimeUsec = AV_NOPTS_VALUE;
    m_lastMotionContainData = false;
}

bool QnServerStreamRecorder::saveMotion(QnAbstractMediaDataPtr media)
{
    QnMetaDataV1Ptr motion = qSharedPointerDynamicCast<QnMetaDataV1>(media);
    if (motion)
        QnMotionHelper::instance()->saveToArchive(motion);
    return true;
}

void QnServerStreamRecorder::beforeProcessData(QnAbstractMediaDataPtr media)
{
    if (m_currentScheduleTask.getRecordingType() == QnScheduleTask::RecordingType_Run)
        return;

    QnMetaDataV1Ptr metaData = qSharedPointerDynamicCast<QnMetaDataV1>(media);
    if (metaData) {
        bool motionContainData = !metaData->isEmpty();
        if (motionContainData || m_lastMotionContainData) {
            m_lastMotionTimeUsec = metaData->timestamp;
            setPrebufferingUsec(0); // flush prebuffer
        }
        m_lastMotionContainData = motionContainData;
        return;
    }
    if (m_lastMotionTimeUsec != AV_NOPTS_VALUE && media->timestamp >= m_lastMotionTimeUsec + m_currentScheduleTask.getAfterThreshold()*1000000ll)
    {
        // no motion sometime. Go to prebuffering mode
        setPrebufferingUsec(m_currentScheduleTask.getBeforeThreshold()*1000000ll);
    }
}


bool QnServerStreamRecorder::needSaveData(QnAbstractMediaDataPtr media)
{
    if (m_currentScheduleTask.getRecordingType() == QnScheduleTask::RecordingType_Run)
        return true;
    else if (m_currentScheduleTask.getRecordingType() == QnScheduleTask::RecordingType_Never)
        return false;

    // if prebuffering mode and all buffer is full - drop data

    bool rez = m_lastMotionTimeUsec != AV_NOPTS_VALUE && media->timestamp < m_lastMotionTimeUsec + m_currentScheduleTask.getAfterThreshold()*1000000ll;
    //qDebug() << "needSaveData=" << rez << "df=" << (media->timestamp - (m_lastMotionTimeUsec + m_currentScheduleTask.getAfterThreshold()*1000000ll))/1000000.0;
    if (!rez)
        close();
    return rez;
}

void QnServerStreamRecorder::updateRecordingType(const QnScheduleTask& scheduleTask)
{
    m_currentScheduleTask = scheduleTask;
    //m_lastMotionTimeUsec = AV_NOPTS_VALUE;
    if (m_currentScheduleTask.getRecordingType() == QnScheduleTask::RecordingType_MotionOnly)
        setPrebufferingUsec(scheduleTask.getBeforeThreshold()*1000000ll);
    else
        setPrebufferingUsec(0);
}

bool QnServerStreamRecorder::processData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return true; // skip data

    // for empty schedule we record all time
    {
        QMutexLocker lock(&m_scheduleMutex);
        if (!m_schedule.isEmpty())
        {
            qint64 timeMs = media->timestamp/1000;

            if (m_skipDataToTime != AV_NOPTS_VALUE && timeMs < m_skipDataToTime)
                return true; // no recording period. skipData

            if (!m_lastSchedulePeriod.containTime(timeMs))
            {
                // find new schedule
                QDateTime dataDate = QDateTime::fromMSecsSinceEpoch(timeMs);
                int scheduleTime = (dataDate.date().dayOfWeek()-1)*3600*24 + dataDate.time().hour()*3600 + dataDate.time().second();

                QnScheduleTaskList::iterator itr = qUpperBound(m_schedule.begin(), m_schedule.end(), scheduleTime);
                if (itr > m_schedule.begin())
                    --itr;

                // truncate current date to a start of week
                dataDate = QDateTime(dataDate.addDays(1 - dataDate.date().dayOfWeek()).date());
                qint64 absoluteScheduleTime = dataDate.toMSecsSinceEpoch() + itr->startTimeMs();

                if (itr->containTime(scheduleTime)) {
                    m_lastSchedulePeriod = QnTimePeriod(absoluteScheduleTime, itr->durationMs()); 
                    updateRecordingType(*itr);
                    m_skipDataToTime = AV_NOPTS_VALUE;
                }
                else {
                    m_skipDataToTime = absoluteScheduleTime;
                    close();
                }
            }
        }
    }

    return QnStreamRecorder::processData(data);
}

void QnServerStreamRecorder::updateSchedule(const QnScheduleTaskList& schedule)
{
    QMutexLocker lock(&m_scheduleMutex);
    m_schedule = schedule;
}
