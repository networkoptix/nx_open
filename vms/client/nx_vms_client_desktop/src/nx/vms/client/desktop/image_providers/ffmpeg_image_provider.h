#pragma once

#include <chrono>

#include <core/resource/resource_fwd.h>

#include "image_provider.h"

namespace nx::vms::client::desktop {

/** Utility class to get an image from the local media file using ffmpeg. */
class FfmpegImageProvider: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    // This one retrives screenshot in the middle of the file.
    explicit FfmpegImageProvider(const QnResourcePtr& resource, const QSize& maximumSize = QSize(),
        QObject* parent = nullptr);

    explicit FfmpegImageProvider(const QnResourcePtr& resource, std::chrono::microseconds position,
        const QSize& maximumSize = QSize(), QObject* parent = nullptr);

    virtual ~FfmpegImageProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    void loadSync() { load(true); }

protected:
    virtual void doLoadAsync() override { load(false); }

private:
    void load(bool sync);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
