#ifndef QN_PLATFORM_IMAGES_H
#define QN_PLATFORM_IMAGES_H

#include <QtCore/QObject>
#include <QtGui/QCursor>

/**
 * The <tt>QnPlatformImages</tt> class is intended for access to platform's standard images.
 */
class QnPlatformImages: public QObject {
    Q_OBJECT
public:
    QnPlatformImages(QObject *parent = NULL): QObject(parent) {}
    virtual ~QnPlatformImages() {}

    /**
     * This function creates a bitmap cursor for the given cursor shape.
     * The result differs from a cursor constructed directly in that its
     * pixmap and hot spot are valid and can be accessed.
     *
     * \param shape                     Standard cursor shape.
     * \returns                         Bitmap cursor for the given standard 
     *                                  cursor shape, or non-bitmap cursor 
     *                                  in case of an error.
     */
    virtual QCursor bitmapCursor(Qt::CursorShape shape) const = 0;

private:
    Q_DISABLE_COPY(QnPlatformImages)
};


#endif // QN_PLATFORM_IMAGES_H
