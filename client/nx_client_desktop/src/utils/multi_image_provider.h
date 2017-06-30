#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>
#include "image_provider.h"
#include <memory>

class QnMultiImageProvider: public QnImageProvider
{
    Q_OBJECT
    using base_type = QnImageProvider;
public:
    typedef std::vector<std::unique_ptr<QnImageProvider>> Providers;

    QnMultiImageProvider(Providers providers, Qt::Orientation orientation, int spacing,
        QObject *parent = 0);
    virtual ~QnMultiImageProvider() {}

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

protected:
    virtual void doLoadAsync() override;
private:
    const Providers m_providers;
    const Qt::Orientation m_orientation;
    const int m_spacing;
    QMap<int, QRect> m_imageRects;
    QImage m_image;
};
