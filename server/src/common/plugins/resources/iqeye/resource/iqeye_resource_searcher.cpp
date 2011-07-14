#include "iqeye_resource_searcher.h"
#include "iqeye_resource.h"

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


QString IQEyeDeviceServer::name() const
{
    return "IQEye";
}


QnResourceList IQEyeDeviceServer::findResources()
{
    QnResourceList lst;
    foreach(Cam cam, mCams)
    {
        QnPlQEyeResourcePtr res ( new QnPlQEyeResource() );
        res->setHostAddress(QHostAddress(cam.ip), false);
        res->setMAC(cam.mac);
        res->setId(res->getMAC());
        res->setName(cam.name);
        res->setAuth("root", "system");
        lst.push_back(res);
    }

    return lst;
}

QnResourcePtr IQEyeDeviceServer::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}