#ifndef __DESKTOP_DEVICE_SERVER_H
#define __DESKTOP_DEVICE_SERVER_H

#include "core/resourcemanagment/resourceserver.h"



struct IDirect3D9;

class DesktopDeviceServer : public QnAbstractResourceSearcher
{
    DesktopDeviceServer();
public:

    ~DesktopDeviceServer();

    static DesktopDeviceServer& instance();

    QString manufacture() const;

    virtual bool isProxy() const { return false; }
    // return the name of the server
    virtual QString name() const { return QLatin1String("Desktop");}

    // returns all available devices
    virtual QnResourceList findResources();

    virtual bool isResourceTypeSupported(const QnId& resourceTypeId) const;
protected:
    virtual QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters);
private:
    IDirect3D9*			m_pD3D;
};

#endif // __DESKTOP_DEVICE_SERVER_H
