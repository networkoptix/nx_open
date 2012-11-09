#ifndef QN_DESKTOP_RESOURCE_H
#define QN_DESKTOP_RESOURCE_H

#include "core/resource/resource.h"

class QnDesktopResource: public QnResource {
    Q_OBJECT;
public:
    QnDesktopResource(int index);

    virtual QString toString() const override;

    virtual QString getUniqueId() const override;

protected:
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(ConnectionRole role) override;
};

#endif // QN_DESKTOP_RESOURCE_H
