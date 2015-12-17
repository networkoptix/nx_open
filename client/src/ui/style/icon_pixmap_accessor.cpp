#include "icon_pixmap_accessor.h"

#include <QtGui/QIcon>

#if QT_VERSION == 0x050201
#include <QtGui/5.2.1/QtGui/private/qicon_p.h>
#elif QT_VERSION == 0x050401
#include <QtGui/5.4.1/QtGui/private/qicon_p.h>
#elif QT_VERSION == 0x050500
#include <QtGui/5.5.0/QtGui/private/qicon_p.h>
#elif QT_VERSION == 0x050501
#include <QtGui/5.5.1/QtGui/private/qicon_p.h>
#elif QT_VERSION == 0x050600
#include <QtGui/5.6.0/QtGui/private/qicon_p.h>
#else
#error "Include proper header here!"
#endif

class QIconThemeEngine {
public:
    static const QVector<QPixmapIconEngineEntry> &pixmaps(const QPixmapIconEngine *engine) {
		return engine->pixmaps;
    }
};

template<class Target>
Target *qiconengine_cast(const QString &key, QIconEngine *source) {
    return source && source->key() == key ? static_cast<Target *>(source) : NULL;
}

QnIconPixmapAccessor::QnIconPixmapAccessor(const QIcon *icon):
	m_icon(icon),
	m_engine(qiconengine_cast<QPixmapIconEngine>(lit("QPixmapIconEngine"), const_cast<QIcon *>(icon)->data_ptr()->engine))
{}

int QnIconPixmapAccessor::size() const {
	return m_engine ? QIconThemeEngine::pixmaps(m_engine).size() : 0;
}

QString QnIconPixmapAccessor::name(int index) const {
	return QIconThemeEngine::pixmaps(m_engine)[index].fileName;
}
