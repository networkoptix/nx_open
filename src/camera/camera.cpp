#include "camera.h"

//#include "../oem/arecontvision/streamreader/cpul_file_test.h"

#include "../base/log.h"
#include "../base/sleep.h"
#include "../device/device.h"
#include "ui/videoitem/video_wnd_item.h"

CLVideoCamera::CLVideoCamera(CLDevice* device, CLVideoWindowItem* videovindow, bool generateEndOfStreamSignal) :
    m_device(device),
    m_videovindow(videovindow),
    m_camdispay(generateEndOfStreamSignal),
    m_recorder(device),
    mGenerateEndOfStreamSignal(generateEndOfStreamSignal)
{
    cl_log.log(QLatin1String("Creating camera for "), m_device->toString(), cl_logDEBUG1);

    int videonum = m_device->getVideoLayout()->numberOfChannels();// how many sensors camera has

    m_stat = new CLStatistics[videonum]; // array of statistics

	for (int i = 0; i < videonum; ++i)
	{
		m_camdispay.addVideoChannel(i, m_videovindow, !m_device->checkDeviceTypeFlag(CLDevice::SINGLE_SHOT));
		m_videovindow->setStatistics(&m_stat[i], i);
	}

	m_reader = m_device->getDeviceStreamConnection();

	m_reader->addDataProcessor(&m_camdispay);
	m_reader->setStatistics(m_stat);

	m_videovindow->setComplicatedItem(this);
	m_videovindow->setInfoText(m_device->toString());

    if (mGenerateEndOfStreamSignal)
        connect(&m_camdispay, SIGNAL( reachedTheEnd() ), this, SLOT( onReachedTheEnd() ));
}

CLVideoCamera::~CLVideoCamera()
{
	cl_log.log(QLatin1String("Destroy camera for "), m_device->toString(), cl_logDEBUG1);

	stopDispay();
	delete m_reader;
	delete[] m_stat;
}

quint64 CLVideoCamera::currentTime() const
{
	return m_camdispay.currentTime();
}

void CLVideoCamera::streamJump(qint64 time)
{
	m_camdispay.jump(time);
}

void CLVideoCamera::startDispay()
{
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::startDispay "), m_device->getUniqueId(), cl_logDEBUG1);

	m_camdispay.start();
	//m_reader->start(QThread::HighestPriority);
	m_reader->start(QThread::HighPriority);
}

void CLVideoCamera::stopDispay()
{
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::stopDispay"), m_device->getUniqueId(), cl_logDEBUG1);
	CL_LOG(cl_logDEBUG1) cl_log.log(QLatin1String("CLVideoCamera::stopDispay reader is about to pleases stop "), QString::number((long)m_reader,16), cl_logDEBUG1);

	stopRecording();

	m_reader->stop();
	m_camdispay.stop();
	m_camdispay.clearUnprocessedData();
}

void CLVideoCamera::beforestopDispay()
{
	m_reader->pleaseStop();
	m_camdispay.pleaseStop();
	m_recorder.pleaseStop();
	m_videovindow->beforeDestroy();
}

void CLVideoCamera::startRecording()
{
	m_reader->setQuality(CLStreamreader::CLSHighest);
	m_reader->addDataProcessor(&m_recorder);
	m_reader->needKeyData();
	m_recorder.start();
	m_videovindow->addSubItem(RecordingSubItem);
}

void CLVideoCamera::stopRecording()
{
	m_recorder.stop();
	m_reader->removeDataProcessor(&m_recorder);
	m_reader->setQuality(CLStreamreader::CLSNormal);
	m_videovindow->removeSubItem(RecordingSubItem);
}

bool CLVideoCamera::isRecording()
{
	return m_recorder.isRunning();
}

CLDevice* CLVideoCamera::getDevice() const
{
	return m_device;
}

CLStreamreader* CLVideoCamera::getStreamreader()
{
	return m_reader;
}

CLCamDisplay* CLVideoCamera::getCamCamDisplay()
{
	return &m_camdispay;
}

CLVideoWindowItem* CLVideoCamera::getVideoWindow()
{
	return m_videovindow;
}

CLAbstractSceneItem* CLVideoCamera::getSceneItem() const
{
	return m_videovindow;
}

const CLVideoWindowItem* CLVideoCamera::getVideoWindow() const
{
	return m_videovindow;
}

CLStatistics* CLVideoCamera::getStatistics()
{
	return m_stat;
}

void CLVideoCamera::setLightCPUMode(bool val)
{
	m_camdispay.setLightCPUMode(val);
}

void CLVideoCamera::setQuality(CLStreamreader::StreamQuality q, bool increase)
{
	if (increase && m_reader->getQuality() >= q)
		return;

	if (!increase && m_reader->getQuality() <= q)
		return;

	if (isRecording())
		m_reader->setQuality(CLStreamreader::CLSHighest);
	else
		m_reader->setQuality(q);
}


void CLVideoCamera::onReachedTheEnd()
{
    if (mGenerateEndOfStreamSignal)
        emit reachedTheEnd();
}
