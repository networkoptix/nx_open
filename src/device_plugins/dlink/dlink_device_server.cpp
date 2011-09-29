#include "dlink_device_server.h"
#include "dlink_device.h"
#include "base/sleep.h"
#include "network/nettools.h"

char request[] = {0xfd, 0xfd, 0x06, 0x00, 0xa1, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
QByteArray barequest(request, sizeof(request));

DlinkDeviceServer::DlinkDeviceServer()
{

}

DlinkDeviceServer::~DlinkDeviceServer()
{

};

DlinkDeviceServer& DlinkDeviceServer::instance()
{
    static DlinkDeviceServer inst;
    return inst;
}

bool DlinkDeviceServer::isProxy() const
{
    return false;
}

QString DlinkDeviceServer::name() const
{
    return QLatin1String("Dlink");
}


CLDeviceList DlinkDeviceServer::findDevices()
{
	CLDeviceList result;

	QList<QHostAddress> ipaddrs = getAllIPv4Addresses();

    
	for (int i = 0; i < ipaddrs.size();++i)
	{
		QUdpSocket sock;
		sock.bind(ipaddrs.at(i), 0);

		// sending broadcast
		
		for (int r = 0; r < 2; ++r)
		{
			sock.writeDatagram(barequest.data(), barequest.size(),QHostAddress::Broadcast, 62976);

			if (r!=1)
				CLSleep::msleep(5);

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

				if (senderPort!=62976 || datagram.size() < 32) // minimum response size
					continue;

				const unsigned char* data = (unsigned char*)(datagram.data());

				unsigned char mac[6];
				memcpy(mac,data + 6,6);

				QString smac = MACToString(mac);
				if (result.find(smac)!=result.end())
					continue; // already found;

				CLDlinkDevice* device = new CLDlinkDevice();
				device->setName(QLatin1String("Dlink"));

				if (device==0)
					continue;

				device->setIP(sender, false);
				device->setMAC(smac);
				device->setUniqueId(smac);

				CLDeviceStatus dh;
				device->setLocalAddr(ipaddrs.at(i));
				device->setStatus(dh);

				result[smac] = device;

			}

			CLSleep::msleep(2); // to avoid 100% cpu usage

		}

	}

	return result;

}
