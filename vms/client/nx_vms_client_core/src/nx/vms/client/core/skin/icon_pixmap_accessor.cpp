// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "icon_pixmap_accessor.h"

#include <QtGui/QIcon>
#include <QtGui/private/qicon_p.h>

class QIconThemeEngine {
public:
    static const QVector<QPixmapIconEngineEntry> &pixmaps(const QPixmapIconEngine *engine) {
        return engine->pixmaps;
    }
};

template<class Target>
Target *qiconengine_cast(const QString &key, QIconEngine *source) {
    return source && source->key() == key ? static_cast<Target *>(source) : nullptr;
}

QnIconPixmapAccessor::QnIconPixmapAccessor(const QIcon *icon):
    m_icon(icon),
    m_engine(qiconengine_cast<QPixmapIconEngine>("QPixmapIconEngine", const_cast<QIcon *>(icon)->data_ptr()->engine))
{}

int QnIconPixmapAccessor::size() const {
    return m_engine ? QIconThemeEngine::pixmaps(m_engine).size() : 0;
}

QString QnIconPixmapAccessor::name(int index) const {
    return QIconThemeEngine::pixmaps(m_engine)[index].fileName;
}
