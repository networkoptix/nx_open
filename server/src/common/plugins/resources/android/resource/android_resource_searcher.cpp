#include "android_resource_searcher.h"
#include "network/simple_http_client.h"
#include "resourcecontrol/resource_pool.h"
#include "android_resource.h"

AndroidDeviceServer::AndroidDeviceServer()
{

}

AndroidDeviceServer::~AndroidDeviceServer()
{

};

AndroidDeviceServer& AndroidDeviceServer::instance()
{
    static AndroidDeviceServer inst;
    return inst;
}


QString AndroidDeviceServer::name() const
{
    return "Android";
}


struct AnDroidDev
{
	// Aluma - All I Need Is Time (Sluslik Luna Mix)
	
	quint32 ip;
	bool android;
	
	void checkIfItAndroid()
	{
		android = false;
		QString request = "";
		
		CLSimpleHTTPClient httpClient(QHostAddress(ip), 8080, 2000, QAuthenticator());
		httpClient.setRequestLine(request);
		httpClient.openStream();
		
		if (httpClient.isOpened())
			android = true;
		
	}
};

QnResourceList AndroidDeviceServer::findResources()
{
    QnResourceList lst;

    QFile file("android.txt"); // Create a file handle for the file named
    if (!file.exists())
        return lst;

    QString line;

    if (!file.open(QIODevice::ReadOnly)) // Open the file
        return lst;

    QTextStream stream(&file); // Set the stream to read from myFile

    while(1)
    {
        line = stream.readLine(); // this reads a line (QString) from the file

        if (line.trimmed()=="")
            break;

        QStringList list = line.split(" ", QString::SkipEmptyParts);


        if (list.count()<2)
            continue;

        QHostAddress min_addr(list.at(0));
        QHostAddress max_addr(list.at(1));


        QList<AnDroidDev> alist;

        quint32 curr = min_addr.toIPv4Address();
        quint32 max_ip = max_addr.toIPv4Address();

        while(curr <= max_ip)
        {
            //QnNetworkResource* nd = QnResourcePool::instance().getDeviceByIp(QHostAddress(curr)); // tyty
            QnNetworkResource* nd;
            if (nd)
            {
                // such res alredy exists;
                //nd->releaseRef();
                ++curr;
                continue;
            }

            AnDroidDev adev;
            adev.ip = curr;
            alist.push_back(adev);

            ++curr;
        }


        int threads = 20;
        QThreadPool* global = QThreadPool::globalInstance();
        for (int i = 0; i < threads; ++i ) global->releaseThread();
        QtConcurrent::blockingMap(alist, &AnDroidDev::checkIfItAndroid);
        for (int i = 0; i < threads; ++i )global->reserveThread();

        foreach(AnDroidDev ad, alist)
        {
            if (ad.android)
            {
                static int n = 0;
                n++;

                QnNetworkResourcePtr res ( new QnPlANdroidResource() );
                res->setIP(QHostAddress(ad.ip), false);
                res->setMAC( QString("android") + QString::number(n) );
                res->setId(res->getMAC());
                res->setName(QString("android") + QString::number(lst.count()+1));
                lst.push_back(res);

            }
        }

    }


    return lst;
}

QnResourcePtr AndroidDeviceServer::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}