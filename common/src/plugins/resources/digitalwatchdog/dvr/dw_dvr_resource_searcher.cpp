#include "dw_dvr_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "dw_dvr_resource.h"
#include "OpnDVRLib.h"

static const int CONNECTION_TIMEOUT = 2000*1000;

DwDvrResourceSearcher::DwDvrResourceSearcher()
{
    bool isInitialized = DVRInitLibrary(false);
    isInitialized = isInitialized;
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
    getCamerasFromDvr(result, "10.10.10.55", 2000, "user1", "123");
    return result;
}

void DwDvrResourceSearcher::getCamerasFromDvr(QnResourceList& resources, const QString& host, int port, const QString& login, const QString& password)
{
    HANDLE dvrHandle = DVROpenConnection();
    if (dvrHandle == 0)
        return;

    DVRSetInstallCode(dvrHandle, -1, 0);

    LPCSTR hostStr =  (LPCSTR) host.data();
    DVRRESULT result = DVRConnectSite(dvrHandle, host.toUtf8(), port, login.toUtf8(), password.toUtf8(), CONNECTION_TIMEOUT);
    switch (result)
    {

    }
}

QnResourcePtr DwDvrResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr();
}
