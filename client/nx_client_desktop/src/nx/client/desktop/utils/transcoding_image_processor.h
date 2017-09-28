#pragma once

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
    nx::core::transcoding::LegacyTranscodingSettings m_settings;
};

} // namespace desktop
} // namespace client
} // namespace nx
