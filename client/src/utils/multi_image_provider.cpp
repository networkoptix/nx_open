#include "multi_image_provider.h"

QnMultiImageProvider::QnMultiImageProvider(Providers providers, Qt::Orientation orientation, int spacing, QObject *parent):
    base_type(parent),
    m_providers(std::move(providers))
{
    for (const auto& provider: m_providers) 
    {
        connect(provider.get(), &QnImageProvider::imageChanged, this, [this, orientation, spacing](const QImage &loadedImage)
        {
            QSize newSize(loadedImage.size());
            QPoint offset(0, 0);

            if (!m_image.isNull()) 
            {
                if (orientation == Qt::Vertical) {
                    newSize = QSize(qMax(loadedImage.width(), m_image.width()), m_image.height() + loadedImage.height() + spacing);
                    offset.setY(m_image.height() + spacing);
                }
                else {
                    newSize = QSize(m_image.width() + loadedImage.width() + spacing, qMax(loadedImage.height(), m_image.height()));
                    offset.setX(m_image.width() + spacing);
                }
            }

            QImage mergedImage(newSize, QImage::Format_ARGB32_Premultiplied);
            mergedImage.fill(Qt::black);
            QPainter p(&mergedImage);
            p.drawImage(0, 0, m_image);
            p.drawImage(offset, loadedImage);
            p.end();
            m_image = mergedImage;

            emit imageChanged(m_image);
        });
    }
}

void QnMultiImageProvider::doLoadAsync()
{
    for (const auto& provider: m_providers)
        provider->loadAsync();
}
