#include "icon_pixmap_accessor.h"

#include <QtGui/QIcon>
#include <QtGui/5.2.1/QtGui/private/qicon_p.h>

class QIconThemeEngine {
public:
    static const QVector<QPixmapIconEngineEntry> &pixmaps(const QPixmapIconEngine *engine) {
		return engine->pixmaps;
    }
};

QnIconPixmapAccessor::QnIconPixmapAccessor(const QIcon *icon):
	m_icon(icon),
	m_engine(dynamic_cast<QPixmapIconEngine *>(const_cast<QIcon *>(icon)->data_ptr()->engine))
{}

int QnIconPixmapAccessor::size() const {
	return m_engine ? QIconThemeEngine::pixmaps(m_engine).size() : 0;
}

QString QnIconPixmapAccessor::name(int index) const {
	return QIconThemeEngine::pixmaps(m_engine)[index].fileName;
}
