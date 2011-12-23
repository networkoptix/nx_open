#include "server_stream_recorder.h"
#include "motion/motion_helper.h"
#include "storage_manager.h"
#include "core/dataprovider/media_streamdataprovider.h"

QnServerStreamRecorder::QnServerStreamRecorder(QnResourcePtr dev):
    QnStreamRecorder(dev)
{
    //m_skipDataToTime = AV_NOPTS_VALUE;
    m_lastMotionTimeUsec = AV_NOPTS_VALUE;
    m_lastMotionContainData = false;
    m_needUpdateStreamParams = false;
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
    if (m_needUpdateStreamParams)
    {
        QnAbstractMediaStreamDataProvider* mediaProvider = dynamic_cast<QnAbstractMediaStreamDataProvider*> (media->dataProvider);
        if (mediaProvider) 
        {
            mediaProvider->setFps(m_currentScheduleTask.getFps());
            mediaProvider->setQuality(m_currentScheduleTask.getStreamQuality());
            m_needUpdateStreamParams = false;
        }
    }
    if (m_currentScheduleTask.getRecordingType() != QnScheduleTask::RecordingType_MotionOnly) {
        return;
    }

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
    if (scheduleTask.getRecordingType() != QnScheduleTask::RecordingType_MotionOnly)
    {
        flushPrebuffer();
        setPrebufferingUsec(0);
    }
    else {
        setPrebufferingUsec(scheduleTask.getBeforeThreshold()*1000000ll);
    }
    m_currentScheduleTask = scheduleTask;
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

            //bool isEmptyPeriod = m_skipDataToTime != AV_NOPTS_VALUE && timeMs < m_skipDataToTime;
            if (!m_lastSchedulePeriod.containTime(timeMs))
            {
                // find new schedule
                QDateTime packetDateTime = QDateTime::fromMSecsSinceEpoch(timeMs);
                QDateTime weekStartDateTime = QDateTime(packetDateTime.addDays(1 - packetDateTime.date().dayOfWeek()).date());
                int scheduleTimeMs = weekStartDateTime.msecsTo(packetDateTime);

                QnScheduleTaskList::iterator itr = qUpperBound(m_schedule.begin(), m_schedule.end(), scheduleTimeMs/1000);
                if (itr > m_schedule.begin())
                    --itr;

                // truncate current date to a start of week
                qint64 absoluteScheduleTime = weekStartDateTime.toMSecsSinceEpoch() + itr->startTimeMs();

                if (itr->containTimeMs(scheduleTimeMs)) {
                    m_lastSchedulePeriod = QnTimePeriod(absoluteScheduleTime, itr->durationMs()); 
                    updateRecordingType(*itr);
                    m_needUpdateStreamParams = true;
                    //m_skipDataToTime = AV_NOPTS_VALUE;
                }
                else {
                    if (timeMs > absoluteScheduleTime)
                        absoluteScheduleTime = weekStartDateTime.addDays(7).toMSecsSinceEpoch() + itr->startTimeMs();
                    //m_skipDataToTime = absoluteScheduleTime;
                    QnScheduleTask noRecordTask(QnId::generateSpecialId(), m_device->getId(), 0, 0, false, QnScheduleTask::RecordingType_Never, 1, 0, 0);
                    qint64 curTime = packetDateTime.toMSecsSinceEpoch();
                    m_lastSchedulePeriod = QnTimePeriod(curTime, absoluteScheduleTime - curTime);
                    updateRecordingType(noRecordTask);
                    //close();
                }
            }
        }
    }

    beforeProcessData(media);

    return QnStreamRecorder::processData(data);
}

void QnServerStreamRecorder::updateSchedule(const QnScheduleTaskList& schedule)
{
    QMutexLocker lock(&m_scheduleMutex);
    m_schedule = schedule;
}

QString QnServerStreamRecorder::fillFileName()
{
    if (m_fixedFileName.isEmpty()) 
    {
        QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(m_device);
        Q_ASSERT_X(netResource != 0, Q_FUNC_INFO, "Only network resources can be used with storage manager!");
        return qnStorageMan->getFileName(m_startDateTime/1000, netResource);
    }
    else {
        return m_fixedFileName;
    }
}

void QnServerStreamRecorder::fileFinished(qint64 durationMs, const QString& fileName)
{
    if (m_truncateInterval != 0)
        qnStorageMan->fileFinished(durationMs, fileName);
};

void QnServerStreamRecorder::fileStarted(qint64 startTimeMs, const QString& fileName)
{
    if (m_truncateInterval > 0) {
        qnStorageMan->fileStarted(startTimeMs, fileName);
    }
}
