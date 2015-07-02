#include "icon_provider.h"

#include <QtCore/QFileSelector>

QnIconProvider::QnIconProvider(QFileSelector *fileSelector) :
    QQuickImageProvider(QQuickImageProvider::Pixmap),
    m_fileSelector(fileSelector),
    m_scale(1.0)
{
}

QPixmap QnIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    QString fileName = lit(":/images/") + id;
    if (m_fileSelector)
        fileName = m_fileSelector->select(fileName);

    QPixmap pixmap(fileName);
    if (pixmap.isNull())
        return pixmap;

    if (!requestedSize.isEmpty())
        pixmap = pixmap.scaled(requestedSize, Qt::KeepAspectRatio);
    else
        pixmap = pixmap.scaled(pixmap.size() * m_scale);

    *size = pixmap.size();
    return pixmap;
}

void QnIconProvider::setScale(qreal scale) {
    m_scale = scale;
}

qreal QnIconProvider::scale() const {
    return m_scale;
}

