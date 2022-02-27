// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/utils/abstract_image_processor.h>

namespace nx { namespace core { namespace transcoding {struct Settings; } } }

namespace nx::vms::client::desktop {

class TranscodingImageProcessor: public AbstractImageProcessor
{
   Q_OBJECT
   using base_type = AbstractImageProcessor;

public:
   explicit TranscodingImageProcessor(QObject* parent = nullptr);
   virtual ~TranscodingImageProcessor() override;

   void setTranscodingSettings(
       const nx::core::transcoding::Settings& settings,
       const QnMediaResourcePtr& resource);

   virtual QSize process(const QSize& sourceSize) const override;
   virtual QImage process(const QImage& sourceImage) const override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
