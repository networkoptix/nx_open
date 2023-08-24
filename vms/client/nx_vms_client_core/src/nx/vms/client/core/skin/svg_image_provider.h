// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQuick/QQuickImageProvider>

namespace nx::vms::client::core {

/**
 * A QML image provider that loads an SVG file located by the specified path and applies the color
 * class customization passed in a URL query. Default branding customization is also applied.
 *
 * An example of a customization URL query: `?primary=dark4&secondary=%23F0F0F0`
 */
class SvgImageProvider: public QQuickImageProvider
{
public:
    explicit SvgImageProvider();
    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;
};

} // namespace nx::vms::client::core
