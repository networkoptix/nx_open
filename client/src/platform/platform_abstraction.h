#ifndef CLIENT_PLATFORM_ABSTRACTION_H
#define CLIENT_PLATFORM_ABSTRACTION_H

#include <QtCore/QObject>

#include <platform/core_platform_abstraction.h>

#include "images/platform_images.h"

class QnPlatformAbstraction: public QnCorePlatformAbstraction {
    Q_OBJECT
    typedef QnCorePlatformAbstraction base_type;

public:
    QnPlatformAbstraction(QObject *parent = 0);
    virtual ~QnPlatformAbstraction();

    QnPlatformImages *images() const {
        return m_images;
    }

private:
    QnPlatformImages *m_images;
};

#undef  qnPlatform
#define qnPlatform (static_cast<QnPlatformAbstraction *>(QnCorePlatformAbstraction::instance()))

#endif // CLIENT_PLATFORM_ABSTRACTION_H
