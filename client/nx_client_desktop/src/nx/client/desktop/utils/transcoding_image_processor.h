#pragma once

#include <QtCore/QScopedPointer>

#include <nx/client/desktop/utils/abstract_image_processor.h>
#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>

namespace nx {
namespace client {
namespace desktop {

class TranscodingImageProcessor: public AbstractImageProcessor
{
   Q_OBJECT
   using base_type = AbstractImageProcessor;

public:
   explicit TranscodingImageProcessor(QObject* parent = nullptr);
   virtual ~TranscodingImageProcessor() override;

   void setTranscodingSettings(const nx::core::transcoding::LegacyTranscodingSettings& settings);

   virtual QSize process(const QSize& sourceSize) const override;
   virtual QImage process(const QImage& sourceImage) const override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
