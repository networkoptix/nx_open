#include "archive_device.h"
#include "archive_stream_reader.h"

CLArchiveDevice::CLArchiveDevice(const QString& arch_path)
{

	addDeviceTypeFlag(CLDevice::RECORDED);

	m_uniqueId = arch_path;
	m_name = QDir(arch_path).dirName();

	QFile file(arch_path + QLatin1String("/layout.xml"));

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
	if (layout_element.tagName() != QLatin1String("layout"))
		return ;

	QString ws = layout_element.attribute(QLatin1String("width"));
	QString hs = layout_element.attribute(QLatin1String("height"));
	mOriginalName = layout_element.attribute(QLatin1String("name"));

	int width = ws.toInt();
	int height = hs.toInt();

	m_videolayout = new CLCustomDeviceVideoLayout(width, height);

	CLCustomDeviceVideoLayout* la = static_cast<CLCustomDeviceVideoLayout*>(m_videolayout);

	QDomNode node = layout_element.firstChild();

	while (!node.isNull())
	{
		if (node.toElement().tagName() == QLatin1String("channel"))
		{
			QString snumber = node.toElement().attribute(QLatin1String("number"));
			QString sh_pos = node.toElement().attribute(QLatin1String("h_pos"));
			QString sv_pos = node.toElement().attribute(QLatin1String("v_pos"));

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

QString CLArchiveDevice::originalName() const
{
	return mOriginalName;
}


CLStreamreader* CLArchiveDevice::getDeviceStreamConnection()
{
	return new CLArchiveStreamReader(this);
}


QString CLArchiveDevice::toString() const
{
	return QLatin1String("recorded:") + m_name;
}
