#pragma once

#include <QtCore/QScopedPointer>

#include <nx/client/desktop/utils/abstract_image_processor.h>

namespace nx { namespace core { namespace transcoding {struct Settings; } } }

struct QnMediaDewarpingParams;

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

   void setTranscodingSettings(
       const nx::core::transcoding::Settings& settings,
       const QnMediaDewarpingParams& mediaDewarpingParams);

   virtual QSize process(const QSize& sourceSize) const override;
   virtual QImage process(const QImage& sourceImage) const override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
