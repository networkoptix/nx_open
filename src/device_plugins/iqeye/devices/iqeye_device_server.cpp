#include "iqeye_device_server.h"
#include "iqeye_device.h"

IQEyeDeviceServer::IQEyeDeviceServer()
{

    QFile file("iqeye.txt"); // Create a file handle for the file named
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
        

        if (list.count()<3)
            continue;

        Cam cam;
        cam.ip = list.at(0);
        cam.name = list.at(1);
        cam.mac = list.at(2);

        mCams.push_back(cam);

    }
    

    

}

IQEyeDeviceServer::~IQEyeDeviceServer()
{

};

IQEyeDeviceServer& IQEyeDeviceServer::instance()
{
    static IQEyeDeviceServer inst;
    return inst;
}

bool IQEyeDeviceServer::isProxy() const
{
    return false;
}

QString IQEyeDeviceServer::name() const
{
    return "IQEye";
}


CLDeviceList IQEyeDeviceServer::findDevices()
{
    CLDeviceList lst;
    foreach(Cam cam, mCams)
    {
        CLIQEyeDevice* dev = new CLIQEyeDevice();
        dev->setIP(QHostAddress(cam.ip), false);
        dev->setMAC(cam.mac);
        dev->setUniqueId(dev->getMAC());
        dev->setName(cam.name);
        dev->setAuth("root", "system");
        lst[dev->getUniqueId()] = dev;
    }

    CLIQEyeDevice::findDevices(lst);

    return lst;
}
