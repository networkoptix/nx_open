#ifndef QN_WINDOWS_IMAGES_H
#define QN_WINDOWS_IMAGES_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN
#include "platform_images.h"
#include <QtGui/QPixmap>

class QnWindowsImages: public QnPlatformImages {
    Q_OBJECT;

    typedef QnPlatformImages base_type;

public:
    QnWindowsImages(QObject *parent = NULL);
    virtual ~QnWindowsImages();
};

#endif // Q_OS_WIN

#endif // QN_WINDOWS_IMAGES_H
