#pragma once

#include <QtCore/QJsonObject>
#include <QtGui/QColor>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace client {
namespace desktop {

struct LayoutTemplateItemSource
{
    QString type;
    QString value;
    bool relativeToBlock = false;
};
#define LayoutTemplateItemSource_Fields (type)(value)(relativeToBlock)

struct LayoutTemplateItem
{
    static const QString kResourceItemType;
    static const QString kZoomWindowItemType;
    static const QString kLocalMediaItemType;
    static const QString kCameraSourceType;
    static const QString kEnhancedCameraSourceType;
    static const QString kFileSourceType;
    static const QString kItemSourceType;

    QString type;
    LayoutTemplateItemSource source;
    bool visible = true;
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
    QColor frameColor;
    // TODO: #dklychkov Use QVariantMap.
    QJsonObject properties;
};
#define LayoutTemplateItem_Fields (type)(source)(visible)(x)(y)(width)(height)(frameColor)(properties)

struct LayoutTemplateBlock
{
    int width = 1;
    int height = 1;
    QList<LayoutTemplateItem> items;
};
#define LayoutTemplateBlock_Fields (width)(height)(items)

struct LayoutTemplate
{
    static const QString kAnalyticsLayoutType;

    QString name;
    QString type;
    QString background;
    LayoutTemplateBlock block;

    bool isValid() const;

    QList<QString> referencedLocalFiles() const;
};
#define LayoutTemplate_Fields (name)(type)(background)(block)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (LayoutTemplateItemSource)(LayoutTemplateItem)(LayoutTemplateBlock)(LayoutTemplate),
    (json))

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::LayoutTemplate)
