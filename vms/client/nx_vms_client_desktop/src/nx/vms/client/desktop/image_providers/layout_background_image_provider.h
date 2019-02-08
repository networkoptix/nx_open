#pragma once

#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

#include "image_provider.h"


namespace nx::vms::client::desktop {

class LayoutBackgroundImageProvider: public Connective<ImageProvider>
{
    Q_OBJECT
    using base_type = Connective<ImageProvider>;

public:
    explicit LayoutBackgroundImageProvider(const QnLayoutResourcePtr& layout,
        const QSize& maxImageSize = QSize(),
        QObject* parent = nullptr);
    virtual ~LayoutBackgroundImageProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
