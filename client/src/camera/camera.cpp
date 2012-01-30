#include "camera.h"
#include "ui/videoitem/video_wnd_item.h"
#include "core/dataprovider/media_streamdataprovider.h"
#include "client_util.h"

CLVideoCamera::CLVideoCamera(QnMediaResourcePtr device, CLVideoWindowItem* videovindow, bool generateEndOfStreamSignal, QnAbstractMediaStreamDataProvider* reader) :
    m_device(device),
    m_videoWindow(videovindow),
    m_camdispay(generateEndOfStreamSignal),
    m_recorder(0),
    m_GenerateEndOfStreamSignal(generateEndOfStreamSignal),
    m_reader(reader),
    m_extTimeSrc(0),
    m_isVisible(true)
{
    cl_log.log(QLatin1String("Creating camera for "), m_device->toString(), cl_logDEBUG1);

    int videonum = device->getVideoLayout(m_reader)->numberOfChannels();// how many sensors camera has

    //m_stat = new QnStatistics[videonum]; // array of statistics

    if(m_videoWindow != NULL) {
	    for (int i = 0; i < videonum; ++i) {
		    m_camdispay.addVideoChannel(i, m_videoWindow, !m_device->checkFlag(QnResource::SINGLE_SHOT));
		    //m_videoWindow->setStatistics(&m_stat[i], i);
        }
	}

	m_reader->addDataProcessor(&m_camdispay);
    //connect(m_reader, SIGNAL(jumpOccured(qint64, bool)), &m_camdispay, SLOT(jump(qint64)), Qt::DirectConnection);
    connect(m_reader, SIGNAL(streamPaused()), &m_camdispay, SLOT(onReaderPaused()), Qt::DirectConnection);
    connect(m_reader, SIGNAL(streamResumed()), &m_camdispay, SLOT(onReaderResumed()), Qt::DirectConnection);
    connect(m_reader, SIGNAL(prevFrameOccured()), &m_camdispay, SLOT(onPrevFrameOccured()), Qt::DirectConnection);
    connect(m_reader, SIGNAL(nextFrameOccured()), &m_camdispay, SLOT(onNextFrameOccured()), Qt::DirectConnection);
	//m_reader->setStatistics(m_stat);

    if(m_videoWindow != NULL) {
	    m_videoWindow->setComplicatedItem(this);
	    m_videoWindow->setInfoText(m_device->toString());
    }

    if (m_GenerateEndOfStreamSignal)
        connect(&m_camdispay, SIGNAL( reachedTheEnd() ), this, SLOT( onReachedTheEnd() ));
}

CLVideoCamera::~CLVideoCamera()
{
	cl_log.log(QLatin1String("Destroy camera for "), m_device->toString(), cl_logDEBUG1);

	stopDisplay();
	delete m_reader;
	//delete[] m_stat;
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
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::startDisplay "), m_device->getUniqueId(), cl_logDEBUG1);

	m_camdispay.start();
	//m_reader->start(QThread::HighestPriority);
	m_reader->start(QThread::HighPriority);
}

void CLVideoCamera::stopDisplay()
{
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::stopDisplay"), m_device->getUniqueId(), cl_logDEBUG1);
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

    if(m_videoWindow != NULL)
	    m_videoWindow->beforeDestroy();
}

void CLVideoCamera::startRecording()
{
	//m_reader->setQuality(QnQualityHighest);
    if (m_recorder == 0) {
        m_recorder = new QnStreamRecorder(m_device);
        QFileInfo fi(m_device->getUniqueId());
        m_recorder->setFileName(getTempRecordingDir() + fi.baseName());
        m_recorder->setTruncateInterval(15);
        connect(m_recorder, SIGNAL(recordingFailed(QString)), this, SIGNAL(recordingFailed(QString)));
    }
	m_reader->addDataProcessor(m_recorder);
	m_reader->setNeedKeyData();
	m_recorder->start();

    if(m_videoWindow != NULL)
	    m_videoWindow->addSubItem(RecordingSubItem);
}

void CLVideoCamera::stopRecording()
{
    if (m_recorder) 
    {
	    m_recorder->stop();
	    m_reader->removeDataProcessor(m_recorder);
    }
	//m_reader->setQuality(QnQualityNormal);

    if(m_videoWindow != NULL)
	    m_videoWindow->removeSubItem(RecordingSubItem);
}

bool CLVideoCamera::isRecording()
{
    return m_recorder ? m_recorder->isRunning() : false;
}

QnResourcePtr CLVideoCamera::getDevice() const
{
	return m_device;
}

QnAbstractStreamDataProvider* CLVideoCamera::getStreamreader()
{
	return m_reader;
}

CLCamDisplay* CLVideoCamera::getCamCamDisplay()
{
	return &m_camdispay;
}

CLVideoWindowItem* CLVideoCamera::getVideoWindow()
{
	return m_videoWindow;
}

CLAbstractSceneItem* CLVideoCamera::getSceneItem() const
{
	return m_videoWindow;
}

const CLVideoWindowItem* CLVideoCamera::getVideoWindow() const
{
	return m_videoWindow;
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
