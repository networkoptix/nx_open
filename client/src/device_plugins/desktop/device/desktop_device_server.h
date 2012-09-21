#ifndef __DESKTOP_DEVICE_SERVER_H
#define __DESKTOP_DEVICE_SERVER_H

#include "core/resource_managment/resource_searcher.h"

struct IDirect3D9;

class DesktopDeviceServer : public QnAbstractResourceSearcher // TODO: rename to resource searcher
{
    DesktopDeviceServer();

public:
    ~DesktopDeviceServer();

    static DesktopDeviceServer& instance();

    virtual QString manufacture() const override;

    virtual QnResourceList findResources() override;

    virtual bool isResourceTypeSupported(QnId resourceTypeId) const override;

protected:
    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters) override;

private:
    IDirect3D9 *m_pD3D;
};

#endif // __DESKTOP_DEVICE_SERVER_H
