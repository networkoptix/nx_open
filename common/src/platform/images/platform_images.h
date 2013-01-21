#ifndef QN_PLATFORM_IMAGES_H
#define QN_PLATFORM_IMAGES_H

#include <QtCore/QObject>
#include <QtGui/QPixmap>

/**
 * @brief The QnPlatformImages class is intended for access to platform's standard images
 */
class QnPlatformImages: public QObject {
    Q_OBJECT
public:
    QnPlatformImages(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformImages() {}

    static QnPlatformImages *newInstance(QObject *parent = NULL);

    virtual QPixmap cursorImage(Qt::CursorShape shape) const = 0;
private:
    Q_DISABLE_COPY(QnPlatformImages)
};


#endif // QN_PLATFORM_IMAGES_H
