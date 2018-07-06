#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>

#include "layout_template.h"

namespace nx {
namespace client {
namespace desktop {

struct LayoutTemplate;

class TemplateLayoutBuilder
{
public:
    struct ItemData
    {
        QnLayoutItemData data;
        LayoutTemplateItem itemTemplate;
        int block = 0;
    };

    TemplateLayoutBuilder();
    ~TemplateLayoutBuilder();

    QnLayoutResourcePtr buildLayout(const LayoutTemplate& layoutTemplate);

    void setResources(const QString& type, const QList<QnResourcePtr>& resources);

    QList<ItemData> createdItems() const;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
