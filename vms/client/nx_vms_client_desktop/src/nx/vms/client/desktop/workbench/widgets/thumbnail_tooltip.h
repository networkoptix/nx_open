// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>

#include "bubble_tooltip.h"

namespace nx::vms::client::core { class ImageProvider; }

namespace nx::vms::client::desktop {

class ThumbnailTooltip: public BubbleToolTip
{
    Q_OBJECT

public:
    ThumbnailTooltip(WindowContext* context);
    virtual ~ThumbnailTooltip() override;

    void setImageProvider(core::ImageProvider* value);
    void setHighlightRect(const QRectF& rect);

    const core::analytics::AttributeList& attributes() const;
    void setAttributes(const core::analytics::AttributeList& value);

    void setMaximumContentSize(const QSize& size);
    void adjustMaximumContentSize(const QModelIndex &index);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
