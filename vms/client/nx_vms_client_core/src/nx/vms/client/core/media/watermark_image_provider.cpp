// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "watermark_image_provider.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/core/watermark/watermark_images.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::client::core {

namespace {

static const QString kScheme("image");
static const QString kWidthTag("width");
static const QString kHeightTag("height");
static const QString kWatermarkTag("watermark");

} // namespace

WatermarkImageProvider::WatermarkImageProvider():
    base_type(QQuickImageProvider::Image)
{
}

WatermarkImageProvider::~WatermarkImageProvider()
{
}

QImage WatermarkImageProvider::requestImage(
    const QString& request,
    QSize* /*size*/,
    const QSize& /*requestedSize*/)
{
    QUrlQuery query(request);
    const QSize sourceSize(
        query.queryItemValue(kWidthTag).toInt(),
        query.queryItemValue(kHeightTag).toInt());
    const nx::core::Watermark watermark = QJson::deserialized<nx::core::Watermark>(
        query.queryItemValue(kWatermarkTag).toLatin1());

    const auto pixmap = nx::core::retrieveWatermarkImage(watermark, sourceSize);
    return pixmap.toImage();
}

QString WatermarkImageProvider::name()
{
    return "watermark";
}

QUrl WatermarkImageProvider::makeWatermarkSourceUrl(
    const nx::core::Watermark& watermark,
    const QSize& size)
{
    QList<QPair<QString, QString>> params;
    params.append({kWidthTag, QString::number(size.width())});
    params.append({kHeightTag, QString::number(size.height())});
    params.append({kWatermarkTag, QJson::serialized(watermark)});

    QUrlQuery query;
    query.setQueryItems(params);

    QUrl result;
    result.setScheme(kScheme);
    result.setHost(WatermarkImageProvider::name());
    result.setQuery(query);
    return result;
}

} // nx::vms::client::core
