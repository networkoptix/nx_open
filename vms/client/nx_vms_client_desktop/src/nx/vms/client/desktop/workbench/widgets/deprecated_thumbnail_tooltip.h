// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "base_tooltip.h"

#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>

class QGraphicsOpacityEffect;

namespace nx::vms::client::desktop {

class TextEditLabel;
class ImageProvider;

class DeprecatedThumbnailTooltip: public BaseTooltip
{
    Q_OBJECT

    using AsyncImageWidget = nx::vms::client::desktop::AsyncImageWidget;

public:
    DeprecatedThumbnailTooltip(QWidget* parent = nullptr);
    DeprecatedThumbnailTooltip(Qt::Edge tailBorder, QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);
    void setThumbnailVisible(bool visible);
    void setImageProvider(nx::vms::client::desktop::ImageProvider* provider);
    void setMaxThumbnailSize(const QSize& value);
    AsyncImageWidget::CropMode cropMode() const;
    void setCropMode(AsyncImageWidget::CropMode value);

    TextEditLabel* textLabel() const { return m_textLabel; }

private:
    nx::vms::client::desktop::TextEditLabel* m_textLabel;
    nx::vms::client::desktop::AsyncImageWidget* m_previewWidget;
    QSize m_maxThumbnailSize;
};

} // namespace nx::vms::client::desktop
