#ifndef MULTI_IMAGE_PROVIDER_H
#define MULTI_IMAGE_PROVIDER_H

#include <QtCore/QObject>
#include <QtGui/QImage>
#include "image_provider.h"
#include <memory>

class QnMultiImageProvider: public QnImageProvider 
{
    Q_OBJECT
    typedef QnImageProvider base_type;
public:
    typedef std::vector<std::unique_ptr<QnImageProvider>> Providers;

    QnMultiImageProvider(Providers providers, Qt::Orientation orientation, int spacing, QObject *parent = 0);
    virtual ~QnMultiImageProvider() {}

    virtual QImage image() const override { return m_image; }
protected:
    virtual void doLoadAsync() override;
private:
    const Providers m_providers;
    QMap<std::ptrdiff_t, QRect> m_imageRects;
    QImage m_image;
};

#endif // MULTI_IMAGE_PROVIDER_H
