#include "server_stream_recorder.h"
#include "motion/motion_helper.h"
#include "storage_manager.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "core/dataprovider/live_stream_provider.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/camera_resource.h"
#include "utils/common/synctime.h"

QnServerStreamRecorder::QnServerStreamRecorder(QnResourcePtr dev, QnResource::ConnectionRole role, QnAbstractMediaStreamDataProvider* mediaProvider):
    QnStreamRecorder(dev),
    m_role(role),
    m_mediaProvider(mediaProvider),
    m_scheduleMutex(QMutex::Recursive)
{
    //m_skipDataToTime = AV_NOPTS_VALUE;
    m_lastMotionTimeUsec = AV_NOPTS_VALUE;
    m_lastMotionContainData = false;
    //m_needUpdateStreamParams = true;
    m_lastWarningTime = 0;
    m_stopOnWriteError = false;
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_motionMaskBinData[i] = (__m128i*) qMallocAligned(MD_WIDTH * MD_HEIGHT/8, 32);
        memset(m_motionMaskBinData[i], 0, MD_WIDTH * MD_HEIGHT/8);
    }
}

QnServerStreamRecorder::~QnServerStreamRecorder()
{
    stop();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) 
        qFreeAligned(m_motionMaskBinData[i]);
}

bool QnServerStreamRecorder::canAcceptData() const
{
    if (!isRunning())
        return true;

    bool rez = QnStreamRecorder::canAcceptData();
    if (!rez) {
        qint64 currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
        if (currentTime - m_lastWarningTime > 1000)
        {
            qWarning() << "HDD/SSD is slow down recording for camera " << m_device->getUniqueId() << "frame rate decreased!";
            m_lastWarningTime = currentTime;
        }
    }
    return rez;
}

void QnServerStreamRecorder::putData(QnAbstractDataPacketPtr data)
{
    if (!isRunning()) {
        return;
    }
    QnStreamRecorder::putData(data);
}

bool QnServerStreamRecorder::saveMotion(QnAbstractMediaDataPtr media)
{
    QnMetaDataV1Ptr motion = qSharedPointerDynamicCast<QnMetaDataV1>(media);
    if (motion)
        QnMotionHelper::instance()->saveToArchive(motion);
    return true;
}

QnScheduleTask QnServerStreamRecorder::currentScheduleTask() const
{
    QMutexLocker lock(&m_scheduleMutex);
    return m_currentScheduleTask;
}

void QnServerStreamRecorder::updateStreamParams()
{
    if (m_mediaProvider && m_role == QnResource::Role_LiveVideo)
    {
        QnLiveStreamProvider* liveProvider = dynamic_cast<QnLiveStreamProvider*>(m_mediaProvider);
        if (m_currentScheduleTask.getRecordingType() != QnScheduleTask::RecordingType_Never) {
            liveProvider->setFps(m_currentScheduleTask.getFps());
            liveProvider->setQuality(m_currentScheduleTask.getStreamQuality());
        }
        else {
            QnPhysicalCameraResourcePtr camera = qSharedPointerDynamicCast<QnPhysicalCameraResource>(m_device);
            Q_ASSERT(camera);
            liveProvider->setFps(camera->getMaxFps()-5);
            liveProvider->setQuality(QnQualityHighest);
        }
        emit fpsChanged(this, m_currentScheduleTask.getFps());
    }
}

void QnServerStreamRecorder::beforeProcessData(QnAbstractMediaDataPtr media)
{
    bool isRecording = m_currentScheduleTask.getRecordingType() != QnScheduleTask::RecordingType_Never;
	if (!m_device->isDisabled()) {
		if (isRecording) {
			if(m_device->getStatus() == QnResource::Online)
				m_device->setStatus(QnResource::Recording);
		}
		else {
			if(m_device->getStatus() == QnResource::Recording)
				m_device->setStatus(QnResource::Online);
		}
	}

    if (m_currentScheduleTask.getRecordingType() != QnScheduleTask::RecordingType_MotionOnly) {
        return;
    }

    QnMetaDataV1Ptr metaData = qSharedPointerDynamicCast<QnMetaDataV1>(media);
    if (metaData) {
        
        metaData->removeMotion(m_motionMaskBinData[metaData->channelNumber]);
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
    {
        close();
        return false;
    }

    // if prebuffering mode and all buffer is full - drop data

    bool rez = m_lastMotionTimeUsec != AV_NOPTS_VALUE && media->timestamp < m_lastMotionTimeUsec + m_currentScheduleTask.getAfterThreshold()*1000000ll;
    //qDebug() << "needSaveData=" << rez << "df=" << (media->timestamp - (m_lastMotionTimeUsec + m_currentScheduleTask.getAfterThreshold()*1000000ll))/1000000.0;
    if (!rez)
        close();
    return rez;
}

void QnServerStreamRecorder::updateRecordingType(const QnScheduleTask& scheduleTask)
{
    QString msg;
    QTextStream str(&msg);
    str << "Update recording params for camera " << m_device->getUniqueId() << "  " << scheduleTask;
    str.flush();
    cl_log.log(msg, cl_logINFO);

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

void QnServerStreamRecorder::updateScheduleInfo(qint64 timeMs)
{
    QMutexLocker lock(&m_scheduleMutex);
    if (!m_schedule.isEmpty())
    {
        //bool isEmptyPeriod = m_skipDataToTime != AV_NOPTS_VALUE && timeMs < m_skipDataToTime;
        if (!m_lastSchedulePeriod.containTime(timeMs))
        {
            // find new schedule
            QDateTime packetDateTime = QDateTime::fromMSecsSinceEpoch(timeMs);
            QDateTime weekStartDateTime = QDateTime(packetDateTime.addDays(1 - packetDateTime.date().dayOfWeek()).date());
            int scheduleTimeMs = weekStartDateTime.msecsTo(packetDateTime);

            QnScheduleTaskList::iterator itr = qUpperBound(m_schedule.begin(), m_schedule.end(), scheduleTimeMs);
            if (itr > m_schedule.begin())
                --itr;

            // truncate current date to a start of week
            qint64 absoluteScheduleTime = weekStartDateTime.toMSecsSinceEpoch() + itr->startTimeMs();

            if (itr->containTimeMs(scheduleTimeMs)) {
                m_lastSchedulePeriod = QnTimePeriod(absoluteScheduleTime, itr->durationMs());
                updateRecordingType(*itr);
                //m_needUpdateStreamParams = true;
                updateStreamParams();
                //m_skipDataToTime = AV_NOPTS_VALUE;
            }
            else {
                if (timeMs > absoluteScheduleTime)
                    absoluteScheduleTime = weekStartDateTime.addDays(7).toMSecsSinceEpoch() + itr->startTimeMs();
                //m_skipDataToTime = absoluteScheduleTime;
                QnScheduleTask noRecordTask(QnId::generateSpecialId(), m_device->getId(), 1, 0, 0, QnScheduleTask::RecordingType_Never, 0, 0);
                qint64 curTime = packetDateTime.toMSecsSinceEpoch();
                m_lastSchedulePeriod = QnTimePeriod(curTime, absoluteScheduleTime - curTime);
                updateRecordingType(noRecordTask);
            }
        }
    }
    else if (m_currentScheduleTask.getRecordingType() != QnScheduleTask::RecordingType_Never) 
    {
        updateRecordingType(QnScheduleTask());
    }
}

bool QnServerStreamRecorder::processData(QnAbstractDataPacketPtr data)
{
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return true; // skip data

    // for empty schedule we record all time
    QMutexLocker lock(&m_scheduleMutex);

    //updateScheduleInfo(media->timestamp/1000);

    beforeProcessData(media);

    return QnStreamRecorder::processData(data);
}

void QnServerStreamRecorder::updateCamera(QnSecurityCamResourcePtr cameraRes)
{
    QMutexLocker lock(&m_scheduleMutex);
    m_schedule = cameraRes->getScheduleTasks();
    Q_ASSERT(((unsigned long)m_motionMaskBinData[0])%16 == 0);
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        QnMetaDataV1::createMask(cameraRes->getMotionMask(i), (char*)m_motionMaskBinData[i]);
    m_lastSchedulePeriod.clear();
    updateScheduleInfo(qnSyncTime->currentMSecsSinceEpoch());
}

QString QnServerStreamRecorder::fillFileName(QnAbstractMediaStreamDataProvider* provider)
{
    if (m_fixedFileName.isEmpty())
    {
        QnNetworkResourcePtr netResource = qSharedPointerDynamicCast<QnNetworkResource>(m_device);
        Q_ASSERT_X(netResource != 0, Q_FUNC_INFO, "Only network resources can be used with storage manager!");
        return qnStorageMan->getFileName(m_startDateTime/1000, netResource, DeviceFileCatalog::prefixForRole(m_role), provider);
    }
    else {
        return m_fixedFileName;
    }
}

void QnServerStreamRecorder::fileFinished(qint64 durationMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider)
{
    if (m_truncateInterval != 0)
        qnStorageMan->fileFinished(durationMs, fileName, provider);
};

void QnServerStreamRecorder::fileStarted(qint64 startTimeMs, const QString& fileName, QnAbstractMediaStreamDataProvider* provider)
{
    if (m_truncateInterval > 0) {
        qnStorageMan->fileStarted(startTimeMs, fileName, provider);
    }
}

void QnServerStreamRecorder::endOfRun()
{
    QnStreamRecorder::endOfRun();
    if(m_device->getStatus() == QnResource::Recording)
		m_device->setStatus(QnResource::Online);
}
