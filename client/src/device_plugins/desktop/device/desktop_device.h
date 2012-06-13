#ifndef __DESKTOP_DEVICE_H
#define __DESKTOP_DEVICE_H

#include "core/resource/resource.h"

class CLDesktopDevice: public QnResource
{
public:
    CLDesktopDevice(int index);

    QString toString() const;

    virtual bool unknownDevice() const;

    virtual QString getUniqueId() const;

protected:
    virtual void initInternal() override {}
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
};

#endif //__DESKTOP_DEVICE_H
