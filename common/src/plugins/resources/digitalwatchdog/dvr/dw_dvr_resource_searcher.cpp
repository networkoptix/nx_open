#include "dw_dvr_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "dw_dvr_resource.h"

#ifdef Q_OS_WIN
#   include "OpnDVRLib.h"
#endif

static const int CONNECTION_TIMEOUT = 2000*1000;

DwDvrResourceSearcher::DwDvrResourceSearcher()
{
#ifdef Q_OS_WIN
    bool isInitialized = DVRInitLibrary(false);
    isInitialized = isInitialized;
#endif
}

DwDvrResourceSearcher::~DwDvrResourceSearcher()
{
#ifdef Q_OS_WIN
    DVRCloseLibrary();
    DVRFreeLibrary();
#endif
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
    return QLatin1String(QnDwDvrResource::MANUFACTURE);
}

QnResourceList DwDvrResourceSearcher::findResources()
{
    QnResourceList result;
    getCamerasFromDvr(result, QLatin1String("10.10.10.55"), 2000, QLatin1String("user1"), QLatin1String("123"));
    return result;
}

void DwDvrResourceSearcher::getCamerasFromDvr(QnResourceList& resources, const QString& host, int port, const QString& login, const QString& password)
{
    Q_UNUSED(resources)
#ifdef Q_OS_WIN32
    HANDLE dvrHandle = DVROpenConnection();
    if (dvrHandle == 0)
        return;

    DVRSetInstallCode(dvrHandle, -1, 0);

    /*LPCSTR hostStr = (LPCSTR) host.data();
    DVRRESULT result =*/
    DVRConnectSite(dvrHandle, host.toUtf8(), port, login.toUtf8(), password.toUtf8(), CONNECTION_TIMEOUT);
    /*switch (result)
    {

    }*/
#endif
}

QnResourcePtr DwDvrResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr();
}
