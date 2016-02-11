#include "multi_image_provider.h"

QnMultiImageProvider::QnMultiImageProvider(Providers providers, Qt::Orientation orientation, int spacing, QObject *parent):
    base_type(parent),
    m_providers(std::move(providers))
{
    for (const auto& provider: m_providers)
    {
        connect(provider.get(), &QnImageProvider::imageChanged, this, [this, orientation, spacing](const QImage &loadedImage)
        {
            if (loadedImage.isNull())
                return;

            enum { kMaxSize = 160 };
            const auto newHeight = std::min<int>(kMaxSize, loadedImage.height());
            const auto newWidth = std::min<int>(kMaxSize, loadedImage.width());
            QSize newSize(newWidth, newHeight);
            QRect rect(0, 0, newWidth, newHeight);
            std::ptrdiff_t key =  (ptrdiff_t) sender();

            if (!m_image.isNull())
            {
                auto existsOffset = m_imageRects.find(key);
                if (existsOffset == m_imageRects.end())
                {
                    if (orientation == Qt::Vertical) {
                        newSize = QSize(qMax(newWidth, m_image.width()), m_image.height() + newHeight + spacing);
                        rect.translate(0, m_image.height() + spacing);
                    }
                    else {
                        newSize = QSize(m_image.width() + newWidth + spacing, qMax(newHeight, m_image.height()));
                        rect.translate(m_image.width() + spacing, 0);
                    }
                }
                else {
                    // paint image at the same place as previous time
                    rect = *existsOffset;
                    newSize = m_image.size();
                }
            }

            m_imageRects[key] = rect;
            QImage mergedImage(newSize, QImage::Format_ARGB32_Premultiplied);
            mergedImage.fill(Qt::black);
            QPainter p(&mergedImage);
            p.drawImage(0, 0, m_image);
            p.drawImage(rect, loadedImage);
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
