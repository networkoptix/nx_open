#ifndef QNICONPROVIDER_H
#define QNICONPROVIDER_H

#include <QtQuick/QQuickImageProvider>

class QFileSelector;

class QnIconProvider : public QQuickImageProvider {
public:
    QnIconProvider(QFileSelector *fileSelector);

    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setScale(qreal scale);
    qreal scale() const;

private:
    QFileSelector *m_fileSelector;
    qreal m_scale;
};

#endif // QNICONPROVIDER_H
