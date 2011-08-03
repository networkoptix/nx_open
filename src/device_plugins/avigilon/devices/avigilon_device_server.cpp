#include "avigilon_device_server.h"
#include "avigilon_device.h"

AVigilonDeviceServer::AVigilonDeviceServer()
{

    QFile file(QLatin1String("clearpix.txt")); // Create a file handle for the file named
    if (!file.exists())
        return;

    QString line;

    if (!file.open(QIODevice::ReadOnly)) // Open the file
        return;

    QTextStream stream(&file); // Set the stream to read from myFile

    while(1)
    {
        line = stream.readLine(); // this reads a line (QString) from the file

        if (line.trimmed().isEmpty())
            break;

        QStringList list = line.split(QLatin1Char(' '), QString::SkipEmptyParts);


        if (list.count()<2)
            continue;

        AvigilonCam cam;
        cam.ip = list.at(0);
        cam.mac = list.at(1);

        mCams.push_back(cam);

    }




}

AVigilonDeviceServer::~AVigilonDeviceServer()
{

};

AVigilonDeviceServer& AVigilonDeviceServer::instance()
{
    static AVigilonDeviceServer inst;
    return inst;
}

bool AVigilonDeviceServer::isProxy() const
{
    return false;
}

QString AVigilonDeviceServer::name() const
{
    return QLatin1String("Avigilon");
}


CLDeviceList AVigilonDeviceServer::findDevices()
{

    //dev->setIP(QHostAddress("192.168.1.99"), false);
    //dev->setMAC("00-18-85-00-C8-72");


    CLDeviceList lst;


    foreach(AvigilonCam cam, mCams)
    {
        CLAvigilonDevice* dev = new CLAvigilonDevice();
        dev->setIP(QHostAddress(cam.ip), false);
        dev->setMAC(cam.mac);
        dev->setUniqueId(dev->getMAC());
        dev->setName(QLatin1String("clearpix") + QString::number(lst.count()+1));
        dev->setAuth(QLatin1String("admin"), QLatin1String("admin"));
        lst[dev->getUniqueId()] = dev;

    }

    return lst;
}
