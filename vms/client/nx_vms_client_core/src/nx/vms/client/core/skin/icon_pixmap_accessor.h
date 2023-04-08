// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_ICON_PIXMAP_ACCESSOR_H
#define QN_ICON_PIXMAP_ACCESSOR_H

#include <QtCore/QString>

class QIcon;
class QPixmapIconEngine;

class QnIconPixmapAccessor {
public:
    QnIconPixmapAccessor(const QIcon *icon);

    int size() const;
    QString name(int index) const;

private:
    const QIcon *m_icon;
    const QPixmapIconEngine *m_engine;
};

#endif // QN_ICON_PIXMAP_ACCESSOR_H
