#pragma once

class QByteArray;
class QQuickView;

namespace nx::client::core {

struct MaxTextureSizeInformer
{
    /**
     * Obtain max texture size when it's possible and write the value to the qml root object
     * property with specified name.
     */
    static void obtainMaxTextureSize(QQuickView* view, const QByteArray& rootObjectPropertyName);

    MaxTextureSizeInformer() = delete;
};

} // namespace nx::client::core
