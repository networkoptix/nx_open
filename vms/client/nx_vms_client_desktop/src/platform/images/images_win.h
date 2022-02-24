// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WINDOWS_IMAGES_H
#define QN_WINDOWS_IMAGES_H

#include "platform_images.h"

class QnWindowsImages: public QnPlatformImages {
    Q_OBJECT
public:
    QnWindowsImages(QObject *parent = nullptr);

    virtual QCursor bitmapCursor(Qt::CursorShape shape) const override;

private:
    QCursor m_moveDragCursor, m_copyDragCursor, m_linkDragCursor;
};


#endif // QN_WINDOWS_IMAGES_H
