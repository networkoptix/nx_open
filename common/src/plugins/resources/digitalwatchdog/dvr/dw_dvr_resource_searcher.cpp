#include "dw_dvr_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "dw_dvr_resource.h"
#include "OpnDVRLib.h"

#include <QAxObject>

static const int CONNECTION_TIMEOUT = 2000*1000;

DwDvrResourceSearcher::DwDvrResourceSearcher()
{
    //bool isInitialized = DVRInitLibrary(false);
}

DwDvrResourceSearcher::~DwDvrResourceSearcher()
{
    DVRCloseLibrary();
    DVRFreeLibrary();
}

DwDvrResourceSearcher& DwDvrResourceSearcher::instance()
{
    static DwDvrResourceSearcher inst;
    return inst;
}

QnResourcePtr DwDvrResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    //result = QnVirtualCameraResourcePtr( new QnDwDvrResource() );
    result = QnVirtualCameraResourcePtr();
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Axis camera resource. TypeID" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString DwDvrResourceSearcher::manufacture() const
{
    return QnDwDvrResource::MANUFACTURE;
}

QnResourceList DwDvrResourceSearcher::findResources()
{
    QnResourceList result;
    getCamerasFromDvr(result, "10.10.10.53", 9000, "admin", "");
    return result;
}

void DwDvrResourceSearcher::getCamerasFromDvr(QnResourceList& resources, const QString& host, int port, const QString& login, const QString& password)
{
    static CLSID const clsid
        = { 0x67815DA3, 0xEC08, 0x41E0, { 0xAE, 0x60, 0x92, 0xE5, 0x93, 0x5E, 0xE8, 0xFB } };

    QAxObject object("{67815DA3-EC08-41E0-AE60-92E5935EE8FB}");
    QVariant res = object.dynamicCall("connect(QString&, quint16, QString&, QString&, int)", host, port, login, password, 65535);
    res = res;



    /*
    bool isInitialized = DVRInitLibrary(false);

    HANDLE dvrHandle = DVROpenConnection();
    if (dvrHandle == 0)
        return;

    DVRSetInstallCode(dvrHandle, -1, 0);

    LPCSTR hostStr =  (LPCSTR) host.data();
    DVRRESULT result = DVRConnectSite(dvrHandle, host.toUtf8(), port, login.toUtf8(), password.toUtf8(), CONNECTION_TIMEOUT);
    switch (result)
    {

    }
    */
}

QnResourcePtr DwDvrResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr();
}
