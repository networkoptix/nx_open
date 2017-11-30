#pragma once

#include <core/resource/resource_fwd.h>

#include <utils/image_provider.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class LayoutBackgroundImageProvider: public Connective<QnImageProvider>
{
    Q_OBJECT
    using base_type = Connective<QnImageProvider>;

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

} // namespace desktop
} // namespace client
} // namespace nx
