#pragma once

#include <core/resource/resource_fwd.h>

#include "image_provider.h"

namespace nx {
namespace client {
namespace desktop {

/** Utility class to get an image from the local media file using ffmpeg. */
class FfmpegImageProvider: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    // This one retrives screenshot in the middle of the file.
    explicit FfmpegImageProvider(const QnResourcePtr& resource, QObject* parent = nullptr);
    explicit FfmpegImageProvider(const QnResourcePtr& resource, qint64 positionUsec,  QObject* parent = nullptr);
    virtual ~FfmpegImageProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

signals:
    void loadDelayed(const QImage& image);

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;
    qint64 m_positionUsec;
};

} // namespace desktop
} // namespace client
} // namespace nx
