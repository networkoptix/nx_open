#include "archive_device.h"
#include "archive_stream_reader.h"

CLArchiveDevice::CLArchiveDevice(const QString& arch_path)
{

	addDeviceTypeFlag(CLDevice::RECORDED);

	m_uniqueId = arch_path;
	m_name = arch_path;

	QFile file(arch_path + "/layout.xml");

	if (!file.exists())
		return ;

	QString errorStr;
	int errorLine;
	int errorColumn;
	QString error;
	QDomDocument doc;

	if (!doc.setContent(&file, &errorStr, &errorLine,&errorColumn)) 
	{
		QTextStream str(&error);
		str << "File custom_layouts.xml" << "; Parse error at line " << errorLine << " column " << errorColumn << " : " << errorStr;
		cl_log.log(error, cl_logERROR);
		return ;
	}

	QDomElement layout_element = doc.documentElement();
	if (layout_element.tagName() != "layout")
		return ;

	QString ws = layout_element.attribute("width");
	QString hs = layout_element.attribute("height");

	int width = ws.toInt();
	int height = hs.toInt();

	m_videolayout = new CLCustomDeviceVideoLayout(width, height);

	CLCustomDeviceVideoLayout* la = static_cast<CLCustomDeviceVideoLayout*>(m_videolayout);

	QDomNode node = layout_element.firstChild();

	while (!node.isNull()) 
	{
		if (node.toElement().tagName() == "channel")
		{
			QString snumber = node.toElement().attribute("number");
			QString sh_pos = node.toElement().attribute("h_pos");
			QString sv_pos = node.toElement().attribute("v_pos");

			la->setChannel(sh_pos.toInt(), sv_pos.toInt(), snumber.toInt());
		}

		node = node.nextSibling();
	}

}

CLArchiveDevice::~CLArchiveDevice()
{

}

void CLArchiveDevice::readdescrfile()
{

}

CLStreamreader* CLArchiveDevice::getDeviceStreamConnection()
{
	return new CLArchiveStreamReader(this);
}