#ifndef __DESKTOP_DEVICE_H
#define __DESKTOP_DEVICE_H

#include "core/resource/resource.h"

class QnDesktopDevice: public QnResource { // TODO: rename to resource
    Q_OBJECT;
public:
    QnDesktopDevice(int index);

    virtual QString toString() const override;

    virtual QString getUniqueId() const override;

protected:
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(ConnectionRole role) override;
};

#endif //__DESKTOP_DEVICE_H
