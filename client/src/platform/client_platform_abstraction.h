#ifndef CLIENT_PLATFORM_ABSTRACTION_H
#define CLIENT_PLATFORM_ABSTRACTION_H

#include <QtCore/QObject>

#include "images/platform_images.h"

class QnClientPlatformAbstraction: public QObject
{
    Q_OBJECT
public:
    explicit QnClientPlatformAbstraction(QObject *parent = 0);
    ~QnClientPlatformAbstraction();

    /**
     * \returns                         Global instance of platform abstraction object.
     *                                  Note that this instance must be created first (e.g. on the stack, like a <tt>QApplication</tt>).
     */
    static QnClientPlatformAbstraction *instance() {
        return s_instance;
    }


    QnPlatformImages *images() const {
        return m_images;
    }

private:
    static QnClientPlatformAbstraction *s_instance;
    QnPlatformImages *m_images;
};

#define qnClientPlatform (QnClientPlatformAbstraction::instance())

#endif // CLIENT_PLATFORM_ABSTRACTION_H
