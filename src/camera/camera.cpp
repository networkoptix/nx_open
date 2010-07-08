#include "camera.h"

//#include "../oem/arecontvision/streamreader/cpul_file_test.h"


#include "../base/log.h"
#include "../base/sleep.h"
#include "../device/device.h"

CLVideoCamera::CLVideoCamera(CLDevice* device, CLVideoWindow* videovindow):
m_device(device),
m_videovindow(videovindow)
{

	m_device->getBaseInfo();

	cl_log.log("Creating camera for ", m_device->toString(), cl_logDEBUG1);


	int videonum = m_device->getVideoLayout()->numberOfChannels();// how many sensors camera has

	m_stat = new CLStatistics[videonum]; // array of statistics

	for (int i = 0; i < videonum; ++i)
	{
		m_camdispay.addVideoChannel(i, m_videovindow);
		m_videovindow->setStatistics(&m_stat[i], i);
	}
	
	m_reader = m_device->getDeviceStreamConnection();

	m_reader->addDataProcessor(&m_camdispay);
	m_reader->setStatistics(m_stat);
}

CLVideoCamera::~CLVideoCamera()
{
	cl_log.log("Destroy camera for ", m_device->toString(), cl_logDEBUG1);

	stopDispay();
	delete m_reader;
	delete[] m_stat;

}

void CLVideoCamera::startDispay()
{
	CL_LOG(cl_logDEBUG1) cl_log.log("CLVideoCamera::startDispay ", m_device->getUniqueId(), cl_logDEBUG1);

	m_camdispay.start();
	//m_reader->start(QThread::HighestPriority);
	m_reader->start(QThread::TimeCriticalPriority);
	
}

void CLVideoCamera::stopDispay()
{
	CL_LOG(cl_logDEBUG1) cl_log.log("CLVideoCamera::stopDispay", m_device->getUniqueId(), cl_logDEBUG1);

	CL_LOG(cl_logDEBUG1) cl_log.log("CLVideoCamera::stopDispay reader is about to pleases stop ", QString::number((int)m_reader,16), cl_logDEBUG1);
	m_reader->stop();
	m_camdispay.stop();
	m_camdispay.clearUnProcessedData();
	
	
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

CLVideoWindow* CLVideoCamera::getVideoWindow()
{
	return m_videovindow;
}

const CLVideoWindow* CLVideoCamera::getVideoWindow() const
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

void CLVideoCamera::coppyImage(bool copy)
{
	m_camdispay.coppyImage(copy);
}


void CLVideoCamera::setQuality(CLStreamreader::StreamQuality q, bool increase)
{
	if (increase && m_reader->getQuality() >= q)
		return;

	if (!increase && m_reader->getQuality() <= q)
		return;


	m_reader->setQuality(q);

}