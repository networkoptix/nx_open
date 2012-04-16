#include "camera.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "client_util.h"
#include "ui/style/skin.h"
#include "core/resource/security_cam_resource.h"

CLVideoCamera::CLVideoCamera(QnMediaResourcePtr resource, bool generateEndOfStreamSignal, QnAbstractMediaStreamDataProvider* reader) :
    m_resource(resource),
    m_camdispay(generateEndOfStreamSignal),
    m_recorder(0),
    m_GenerateEndOfStreamSignal(generateEndOfStreamSignal),
    m_reader(reader),
    m_extTimeSrc(0),
    m_isVisible(true),
    m_exportRecorder(0),
    m_exportReader(0)
{
    cl_log.log(QLatin1String("Creating camera for "), m_resource->toString(), cl_logDEBUG1);

    int videonum = resource->getVideoLayout(m_reader)->numberOfChannels();// how many sensors camera has

    //m_stat = new QnStatistics[videonum]; // array of statistics

	m_reader->addDataProcessor(&m_camdispay);
    //connect(m_reader, SIGNAL(jumpOccured(qint64, bool)), &m_camdispay, SLOT(jump(qint64)), Qt::DirectConnection);
    connect(m_reader, SIGNAL(streamPaused()), &m_camdispay, SLOT(onReaderPaused()), Qt::DirectConnection);
    connect(m_reader, SIGNAL(streamResumed()), &m_camdispay, SLOT(onReaderResumed()), Qt::DirectConnection);
    connect(m_reader, SIGNAL(prevFrameOccured()), &m_camdispay, SLOT(onPrevFrameOccured()), Qt::DirectConnection);
    connect(m_reader, SIGNAL(nextFrameOccured()), &m_camdispay, SLOT(onNextFrameOccured()), Qt::DirectConnection);

    connect(m_reader, SIGNAL(slowSourceHint()), &m_camdispay, SLOT(onSlowSourceHint()), Qt::DirectConnection);
    connect(m_reader, SIGNAL(beforeJump(qint64)), &m_camdispay, SLOT(onBeforeJump(qint64)), Qt::DirectConnection);
    connect(m_reader, SIGNAL(jumpOccured(qint64)), &m_camdispay, SLOT(onJumpOccured(qint64)), Qt::DirectConnection);
    connect(m_reader, SIGNAL(jumpCanceled(qint64)), &m_camdispay, SLOT(onJumpCanceled(qint64)), Qt::DirectConnection);
    

	//m_reader->setStatistics(m_stat);

    if (m_GenerateEndOfStreamSignal)
        connect(&m_camdispay, SIGNAL( reachedTheEnd() ), this, SLOT( onReachedTheEnd() ));
}

CLVideoCamera::~CLVideoCamera()
{
	cl_log.log(QLatin1String("Destroy camera for "), m_resource->toString(), cl_logDEBUG1);

	stopDisplay();
	delete m_reader;
	//delete[] m_stat;
}

QnMediaResourcePtr CLVideoCamera::resource() {
    return m_resource;
}

qint64 CLVideoCamera::getCurrentTime() const
{
    if (m_extTimeSrc && m_extTimeSrc->isEnabled())
	    return m_extTimeSrc->getDisplayedTime();
    else
        return m_camdispay.getDisplayedTime();
}

/*
void CLVideoCamera::streamJump(qint64 time)
{
	m_camdispay.jump(time);
}
*/

void CLVideoCamera::startDisplay()
{
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::startDisplay "), m_resource->getUniqueId(), cl_logDEBUG1);

	m_camdispay.start();
	//m_reader->start(QThread::HighestPriority);
	m_reader->start(QThread::HighPriority);
}

void CLVideoCamera::stopDisplay()
{
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::stopDisplay"), m_resource->getUniqueId(), cl_logDEBUG1);
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::stopDisplay reader is about to pleases stop "), QString::number((long)m_reader,16), cl_logDEBUG1);

	stopRecording();

	m_reader->stop();
	m_camdispay.stop();
	m_camdispay.clearUnprocessedData();
}

void CLVideoCamera::beforeStopDisplay()
{
	m_reader->pleaseStop();
	m_camdispay.pleaseStop();
    if (m_recorder)
	    m_recorder->pleaseStop();

}

void CLVideoCamera::startRecording()
{
	//m_reader->setQuality(QnQualityHighest);
    if (m_recorder == 0) {
        m_recorder = new QnStreamRecorder(m_resource);
        QFileInfo fi(m_resource->getUniqueId());
        m_recorder->setFileName(getTempRecordingDir() + fi.baseName());
        m_recorder->setTruncateInterval(15);
        connect(m_recorder, SIGNAL(recordingFailed(QString)), this, SIGNAL(recordingFailed(QString)));
    }
	m_reader->addDataProcessor(m_recorder);
	m_reader->setNeedKeyData();
	m_recorder->start();
}

void CLVideoCamera::stopRecording()
{
    if (m_recorder) 
    {
	    m_recorder->stop();
	    m_reader->removeDataProcessor(m_recorder);
    }
	//m_reader->setQuality(QnQualityNormal);
}

bool CLVideoCamera::isRecording()
{
    return m_recorder ? m_recorder->isRunning() : false;
}

QnResourcePtr CLVideoCamera::getDevice() const
{
	return m_resource;
}

QnAbstractStreamDataProvider* CLVideoCamera::getStreamreader()
{
	return m_reader;
}

CLCamDisplay* CLVideoCamera::getCamDisplay()
{
	return &m_camdispay;
}

const QnStatistics* CLVideoCamera::getStatistics(int channel)
{
	return m_reader->getStatistics(channel);
}

void CLVideoCamera::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
	m_camdispay.setLightCPUMode(val);
}

void CLVideoCamera::setQuality(QnStreamQuality q, bool increase)
{
    /*
	if (increase && m_reader->getQuality() >= q)
		return;

	if (!increase && m_reader->getQuality() <= q)
		return;

	if (isRecording())
		m_reader->setQuality(QnQualityHighest);
	else
		m_reader->setQuality(q);
        /**/
}


void CLVideoCamera::onReachedTheEnd()
{
    if (m_GenerateEndOfStreamSignal)
        emit reachedTheEnd();
}

void CLVideoCamera::exportMediaPeriodToFile(qint64 startTime, qint64 endTime, const QString& fileName, const QString& format)
{
    if (startTime > endTime)
        qSwap(startTime, endTime);

    if (m_exportRecorder == 0)
    {
        QnAbstractStreamDataProvider* tmpReader = m_resource->createDataProvider(QnResource::Role_Default);
        m_exportReader = dynamic_cast<QnAbstractArchiveReader*> (tmpReader);
        if (m_exportReader == 0)
        {
            delete tmpReader;
            emit recordingFailed("Invalid resource type for export data");
            return;
        }

        m_exportRecorder = new QnStreamRecorder(m_resource);
        m_exportRecorder->disconnect(this);
        connect(m_exportRecorder, SIGNAL(recordingFailed(QString)), this, SIGNAL(exportFailed(QString)));
        connect(m_exportRecorder, SIGNAL(recordingFinished(QString)), this, SLOT(onExportFinished(QString)));
        connect(m_exportRecorder, SIGNAL(recordingProgress(int)), this, SIGNAL(exportProgress(int)));
    }

    m_exportRecorder->clearUnprocessedData();
    m_exportRecorder->setEofDateTime(endTime);
    m_exportRecorder->setFileName(fileName);
    m_exportRecorder->setRole(QnStreamRecorder::Role_FileExport);
    m_exportRecorder->setContainer(format);

#ifndef _DEBUG
    if (qSharedPointerDynamicCast<QnSecurityCamResource>(m_resource))
#endif
    {
        m_exportRecorder->setNeedCalcSignature(true);
        m_exportRecorder->setSignLogo(qnSkin->pixmap("logo_1920_1080.png"));
    }

    m_exportReader->addDataProcessor(m_exportRecorder);
    m_exportReader->jumpTo(startTime, startTime);

    m_exportReader->start();
    m_exportRecorder->start();
}

void CLVideoCamera::onExportFinished(QString fileName)
{
    stopExport();
    emit exportFinished(fileName);
}

void CLVideoCamera::stopExport()
{
    if (m_exportReader)
        m_exportReader->stop();
    if (m_exportRecorder)
        m_exportRecorder->stop();
    delete m_exportReader;
    delete m_exportRecorder;
    m_exportReader = 0;
    m_exportRecorder = 0;
}
