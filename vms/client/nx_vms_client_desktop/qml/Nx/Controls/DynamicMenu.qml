// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

Menu
{
    id: dynamicMenu

    property alias /*AbstractItemModel*/ model: dynamicItems.model
    property alias /*var*/ rootIndex: dynamicItems.rootIndex

    property alias /*Component*/ separator: dynamicItems.separator

    property alias /*string*/ textRole: dynamicItems.textRole
    property alias /*string*/ dataRole: dynamicItems.dataRole
    property alias /*string*/ decorationPathRole: dynamicItems.decorationPathRole
    property alias /*string*/ shortcutRole: dynamicItems.shortcutRole
    property alias /*string*/ enabledRole: dynamicItems.enabledRole
    property alias /*string*/ visibleRole: dynamicItems.visibleRole

    DynamicMenuItems
    {
        id: dynamicItems
        parentMenu: dynamicMenu
    }
}
