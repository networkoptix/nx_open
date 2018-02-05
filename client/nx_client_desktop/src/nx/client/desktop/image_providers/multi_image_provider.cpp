#include "multi_image_provider.h"

#include <QtGui/QPainter>

#include <client/client_globals.h>
#include <nx/client/core/utils/geometry.h>

using nx::client::core::Geometry;

namespace {

static const QPoint kOffset(0, 0);
static const QSize kMaxSize(400, 160);

} // namespace

QnMultiImageProvider::QnMultiImageProvider(
    Providers providers,
    Qt::Orientation orientation,
    int spacing,
    QObject* parent)
    :
    base_type(parent),
    m_providers(std::move(providers)),
    m_orientation(orientation),
    m_spacing(spacing)
{
    int key = 0;
    for (const auto& provider: m_providers)
    {
        connect(provider.get(), &QnImageProvider::imageChanged, this,
            [this, key](const QImage &loadedImage)
            {
                if (loadedImage.isNull())
                    return;

                QSize newSize = Geometry::bounded(loadedImage.size(), kMaxSize, Qt::KeepAspectRatio).toSize();
                QRect rect(kOffset, newSize);

                if (!m_image.isNull())
                {
                    auto existsOffset = m_imageRects.find(key);
                    if (existsOffset == m_imageRects.end())
                    {
                        if (m_orientation == Qt::Vertical)
                        {
                            newSize = QSize(qMax(newSize.width(), m_image.width()),
                                m_image.height() + newSize.height() + m_spacing);
                            rect.translate(0, m_image.height() + m_spacing);
                        }
                        else
                        {
                            newSize = QSize(m_image.width() + newSize.width() + m_spacing,
                                qMax(newSize.height(), m_image.height()));
                            rect.translate(m_image.width() + m_spacing, 0);
                        }
                    }
                    else
                    {
                        // paint image at the same place as previous time
                        rect = *existsOffset;
                        newSize = m_image.size();
                    }
                }

                m_imageRects[key] = rect;
                QImage mergedImage(newSize, QImage::Format_ARGB32_Premultiplied);
                mergedImage.fill(Qt::transparent);
                QPainter p(&mergedImage);
                p.drawImage(0, 0, m_image);
                p.drawImage(rect, loadedImage);
                p.end();
                m_image = mergedImage;

                emit sizeHintChanged(sizeHint());
                emit imageChanged(m_image);
            });

        connect(provider.get(), &QnImageProvider::statusChanged, this,
            [this] {emit statusChanged(status()); });
        connect(provider.get(), &QnImageProvider::sizeHintChanged, this,
            [this] { emit sizeHintChanged(sizeHint()); });

        ++key;
    }
}

QImage QnMultiImageProvider::image() const
{
    return m_image;
}

QSize QnMultiImageProvider::sizeHint() const
{
    if (!m_image.isNull())
        return m_image.size();

    if (m_providers.empty())
        return QSize();

    return Geometry::bounded(m_providers.front()->sizeHint(), kMaxSize, Qt::KeepAspectRatio).toSize();
}

Qn::ThumbnailStatus QnMultiImageProvider::status() const
{
    // TODO: #gdm improve logic, statusChanged
    for (const auto& provider: m_providers)
    {
        if (provider->status() == Qn::ThumbnailStatus::Loaded)
            return Qn::ThumbnailStatus::Loaded;
    }
    return Qn::ThumbnailStatus::Loading;
}

void QnMultiImageProvider::doLoadAsync()
{
    for (const auto& provider: m_providers)
        provider->loadAsync();
}
