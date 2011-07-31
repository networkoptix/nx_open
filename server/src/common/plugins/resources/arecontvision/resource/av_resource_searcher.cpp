#include "av_resource_searcher.h"
#include "av_resource.h"
#include "../tools/AVJpegHeader.h"


extern const char* ArecontVisionManufacture;

QnPlArecontResourceSearcher::QnPlArecontResourceSearcher()
{
	// everything related to Arecont must be initialized here
	AVJpeg::Header::Initialize("ArecontVision", "CamLabs", "ArecontVision");

	QString error;
	if (QnPlAreconVisionResource::loadDevicesParam(QCoreApplication::applicationDirPath() + "/arecontvision/devices.xml", error))
	{
		CL_LOG(cl_logINFO)
		{
			QString msg;
			QTextStream str(&msg) ;
			QStringList lst = QnResource::supportedDevises();
			str << "Ssupported devices loaded; size = " << lst.size() << ": " << endl << lst.join("\n");
			cl_log.log(msg, cl_logINFO);
		}
	}
	else
	{
		CL_LOG(cl_logERROR)
		{
			QString log  = "Cannot load devices list. Error:";
			log+=error;
			cl_log.log(log, cl_logERROR);
		}

	}

}


QString QnPlArecontResourceSearcher::manufacture() const
{
	return ArecontVisionManufacture;
}

// returns all available devices 
QnResourceList QnPlArecontResourceSearcher::findResources()
{
	QnResourceList result;

	QList<QHostAddress> ipaddrs = getAllIPv4Addresses();

	CL_LOG(cl_logDEBUG1)
	{
		QString log;
		QTextStream(&log) << "CLAreconVisionDevice::findDevices  found " << ipaddrs.size() << " adapter(s) with IPV4";
		cl_log.log(log, cl_logDEBUG1);

		for (int i = 0; i < ipaddrs.size();++i)
		{
			QString slog;
			QTextStream(&slog) << ipaddrs.at(i).toString();
			cl_log.log(slog, cl_logDEBUG1);
		}
	}

	for (int i = 0; i < ipaddrs.size();++i)
	{
		QUdpSocket sock;
		sock.bind(ipaddrs.at(i), 0);

		// sending broadcast
		QByteArray datagram = "Arecont_Vision-AV2000\1";
		for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
		{
			sock.writeDatagram(datagram.data(), datagram.size(),QHostAddress::Broadcast, 69);

			if (r!=CL_BROAD_CAST_RETRY-1)
				QnSleep::msleep(5);

		}

		// collecting response 
		QTime time;
		time.start();

		while(time.elapsed()<150)
		{
			while (sock.hasPendingDatagrams()) 
			{
				QByteArray datagram;
				datagram.resize(sock.pendingDatagramSize());

				QHostAddress sender;
				quint16 senderPort;

				sock.readDatagram(datagram.data(), datagram.size(),	&sender, &senderPort);

				if (senderPort!=69 || datagram.size() < 32) // minimum response size
					continue;

				const unsigned char* data = (unsigned char*)(datagram.data());
				if (memcmp(data, "Arecont_Vision-AV2000", 21 )!=0)
					continue; // this responde id not from arecont camera

				unsigned char mac[6];
				memcpy(mac,data + 22,6);

                /*/
				QString smac = MACToString(mac);

				
                QString id = "AVUNKNOWN";
				int model = 0;

				
				int shift = 32;

				CLAreconVisionDevice* resource = 0;

				if (datagram.size() > shift + 5)
				{
					model = (unsigned char)data[shift+2] * 256 + (unsigned char)data[shift+3]; //4
					QString smodel; 
					smodel.setNum(model);
					smodel = smodel;
					id = smodel; // this is not final version of the ID; it might/must be updated later 
					resource = deviceByID(id, model);

					if (resource)
						resource->setName(id);
				}
				else
				{
					// very old cam; in future need to request model seporatly 
					resource = new CLAreconVisionDevice(AVUNKNOWN);
					resource->setName("AVUNKNOWN");

				}
				/**/

				// in any case let's HTTP do it's job at very end of discovery 
				QnNetworkResourcePtr resource ( new QnPlAreconVisionResource() );
				//resource->setName("AVUNKNOWN");

				if (resource==0)
					continue;

				resource->setHostAddress(sender, QnDomainPhysical);
				resource->setMAC(mac);
				resource->setDiscoveryAddr( ipaddrs.at(i) );

                if (hasEqualResource(result, resource))
                    continue; // already has such 

				result.push_back(resource);
			}

			QnSleep::msleep(2); // to avoid 100% cpu usage

		}

	}

	return result;

}

QnPlArecontResourceSearcher& QnPlArecontResourceSearcher::instance()
{
	static QnPlArecontResourceSearcher inst;
	return inst;
}

QnResourcePtr QnPlArecontResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}