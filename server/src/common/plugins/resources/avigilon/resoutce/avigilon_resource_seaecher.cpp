#include "avigilon_resource_seaecher.h"
#include "avigilon_resource.h"

AVigilonDeviceServer::AVigilonDeviceServer()
{

    QFile file("clearpix.txt"); // Create a file handle for the file named
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


QString AVigilonDeviceServer::name() const
{
    return "Avigilon";
}


QnResourceList AVigilonDeviceServer::findResources()
{
    QnResourceList lst;

    foreach(AvigilonCam cam, mCams)
    {
        QnPlAvigilonResourcePtr resource ( new QnPlAvigilonResource() );
        resource->setIP(QHostAddress(cam.ip), false);
        resource->setMAC(cam.mac);
        resource->setUniqueId(resource->getMAC());
        resource->setName(QString("clearpix") + QString::number(lst.count()+1));
        resource->setAuth("admin", "admin");
        lst.push_back(resource);
    }

    return lst;
}

QnResourcePtr AVigilonDeviceServer::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}