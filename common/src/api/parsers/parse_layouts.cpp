#include "api/parsers/parse_layouts.h"

#include "core/resourcemanagment/asynch_seacher.h"

void parseLayouts(QnLayoutDataList& layouts, const QnApiLayouts& xsdLayouts)
{
    using xsd::api::layouts::Layouts;
    using xsd::api::layouts::Items;

    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Layouts::layout_const_iterator i (xsdLayouts.begin()); i != xsdLayouts.end(); ++i)
    {
        QnResourceParameters parameters;
        parameters["id"] = i->id().c_str();
        parameters["name"] = i->name().c_str();

        QnLayoutDataPtr layout(new QnLayoutData());
        layout->id = i->id().c_str();
        layout->name = i->name().c_str();

        if (i->items().present())
        {
            const Items::item_sequence& items = (*(i->items())).item();
            for (Items::item_const_iterator ci(items.begin()); ci != items.end(); ++ci)
            {
                QnLayoutItemData itemData;
                itemData.flags = ci->flags();
                itemData.combinedGeometry.setLeft(ci->left());
                itemData.combinedGeometry.setTop(ci->top());
                itemData.combinedGeometry.setRight(ci->right());
                itemData.combinedGeometry.setBottom(ci->bottom());
                itemData.rotation = ci->rotation();

                layout->items.append(itemData);
            }
        }


        layouts.append(layout);
    }
}

