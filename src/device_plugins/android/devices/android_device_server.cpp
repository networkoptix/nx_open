#include "android_device_server.h"
#include "android_device.h"

AndroidDeviceServer::AndroidDeviceServer()
{

    QFile file("android.txt"); // Create a file handle for the file named
    if (!file.exists())
        return;

    QString line;

    if (!file.open(QIODevice::ReadOnly)) // Open the file
        return;

    QTextStream stream(&file); // Set the stream to read from myFile

    while(1)
    {
        line = stream.readLine(); // this reads a line (QString) from the file

        if (line.trimmed()=="")
            break;

        QStringList list = line.split(" ", QString::SkipEmptyParts);
        

        if (list.count()<2)
            continue;

        AndroidCam cam;
        cam.ip = list.at(0);
        cam.mac = list.at(1);

        mCams.push_back(cam);

    }
    

    

}

AndroidDeviceServer::~AndroidDeviceServer()
{

};

AndroidDeviceServer& AndroidDeviceServer::instance()
{
    static AndroidDeviceServer inst;
    return inst;
}

bool AndroidDeviceServer::isProxy() const
{
    return false;
}

QString AndroidDeviceServer::name() const
{
    return "Android";
}


CLDeviceList AndroidDeviceServer::findDevices()
{
    CLDeviceList lst;
    foreach(AndroidCam cam, mCams)
    {
        CLANdroidDevice* dev = new CLANdroidDevice();
        dev->setIP(QHostAddress(cam.ip), false);
        dev->setMAC(cam.mac);
        dev->setUniqueId(dev->getMAC());
        dev->setName(QString("android") + QString::number(lst.count()+1));
        lst[dev->getUniqueId()] = dev;
    }

    return lst;
}
