#pragma once

#include <core/resource/resource_fwd.h>

#include "image_provider.h"

namespace nx {
namespace client {
namespace desktop {

/** Utility class to get an image from the local media file using ffmpeg. */
class FfmpegImageProvider: public QnImageProvider
{
    Q_OBJECT
    using base_type = QnImageProvider;

public:
    explicit FfmpegImageProvider(const QnResourcePtr& resource, QObject* parent = nullptr);
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
};

} // namespace desktop
} // namespace client
} // namespace nx
