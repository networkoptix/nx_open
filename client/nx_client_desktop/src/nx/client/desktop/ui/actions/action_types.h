#pragma once

#include <nx/client/desktop/condition/condition_types.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

/**
* Scope of an action.
*
* Scope defines the menus in which an action can appear, and target
* for getting the action's parameters in case it was triggered with a
* hotkey.
*/
enum ActionScope
{
    InvalidScope = 0x00000000,
    MainScope = 0x00000001,             /**< Action appears in main menu. */
    SceneScope = 0x00000002,            /**< Action appears in scene context menu and its parameters are taken from the scene. */
    TreeScope = 0x00000004,             /**< Action appears in tree context menu. */
    TimelineScope = 0x00000008,         /**< Action appears in slider context menu. */
    TitleBarScope = 0x00000010,         /**< Action appears title bar context menu. */
    NotificationsScope = 0x00000020,
    ScopeMask = 0x000000FF
};
Q_DECLARE_FLAGS(ActionScopes, ActionScope)

/**
* Type of an action parameter.
*
* Note that some of these types are convertible to other types.
*/
enum ActionParameterType
{
    ResourceType = 0x00000100,          /**< Resource, <tt>QnResourcePtr</tt>. */
    LayoutItemType = 0x00000200,        /**< Layout item, <tt>QnLayoutItemIndex</tt>. Convertible to resource. */
    WidgetType = 0x00000400,            /**< Resource widget, <tt>QnResourceWidget *</tt>. Convertible to layout item and resource. */
    LayoutType = 0x00000800,            /**< Workbench layout, <tt>QnWorkbenchLayout *</tt>. Convertible to resource. */
    VideoWallItemType = 0x00001000,     /**< Videowall item, <tt>QnVideoWallItemIndex</tt>. Convertible to resource. */
    VideoWallMatrixType = 0x00002000,   /**< Videowall matrix, <tt>QnVideoWallMatrixIndex</tt>. */
    OtherType = 0x00004000,             /**< Some other type. */
    TargetTypeMask = 0x0000FF00
};
Q_DECLARE_FLAGS(ActionParameterTypes, ActionParameterType)

enum ActionFlag
{
    /** Action can be applied when there are no targets. */
    NoTarget = 0x00010000,

    /** Action can be applied to a single target. */
    SingleTarget = 0x00020000,

    /** Action can be applied to multiple targets. */
    MultiTarget = 0x00040000,

    /** Action accepts resources as target. */
    ResourceTarget = ResourceType,

    /** Action accepts layout items as target. */
    LayoutItemTarget = LayoutItemType,

    /** Action accepts resource widgets as target. */
    WidgetTarget = WidgetType,

    /** Action accepts workbench layouts as target. */
    LayoutTarget = LayoutType,

    /** Action accepts videowall items as target. */
    VideoWallItemTarget = VideoWallItemType,

    /** Action accepts videowall matrices as target. */
    VideoWallMatrixTarget = VideoWallMatrixType,

    Separator = NoTarget | SingleTarget | MultiTarget
        | WidgetTarget | ResourceTarget | LayoutItemTarget | VideoWallItemTarget,

    /** Action has a hotkey that is intentionally ambiguous.
    * It is up to the user to ensure that proper action conditions make it
    * impossible for several actions to be triggered by this hotkey. */
    IntentionallyAmbiguous = 0x00100000,

    /** When the action is activated via hotkey, its scope should not be compared to the current one.
    * Action can be executed from any scope, and its target will be taken from its scope. */
    ScopelessHotkey = 0x00200000,

    /** When the action is activated via hotkey, it will be executed with an empty target. */
    TargetlessHotkey = 0x04000000,

    /** Action can be pulled into enclosing menu if it is the only one in
    * its submenu. It may have different text in this case. */
    Pullable = 0x00400000,

    /** Action is not present in its corresponding menu. */
    HotkeyOnly = 0x00800000,

    /** Action must have at least one child to appear in menu. */
    RequiresChildren = 0x01000000,

    /** Action can be executed only in dev-mode. */
    DevMode = 0x02000000,

    GlobalHotkey = NoTarget | ScopelessHotkey | TargetlessHotkey,


    /** Action can appear in main menu. */
    Main = MainScope | NoTarget,

    /** Action can appear in scene context menu. */
    Scene = SceneScope | WidgetTarget,

    /** Action can appear in scene context menu in videowall review mode (target elements are not resource widgets). */
    VideoWallReviewScene = SceneScope,

    /** Action can appear in tree context menu. */
    Tree = TreeScope,

    /** Action can appear in slider context menu. */
    Slider = TimelineScope | WidgetTarget,

    /** Action can appear in title bar context menu. */
    TitleBar = TitleBarScope | LayoutTarget,

    Notifications = NotificationsScope | WidgetTarget
};
Q_DECLARE_FLAGS(ActionFlags, ActionFlag)

enum ActionVisibility
{
    /** Action is not in the menu. */
    InvisibleAction,

    /** Action is in the menu, but is disabled. */
    DisabledAction,

    /** Action is in the menu and can be triggered. */
    EnabledAction
};

enum ClientMode
{
    DesktopMode = 0x1,
    VideoWallMode = 0x2,
    ActiveXMode = 0x4,
    AnyMode = 0xFF
};
Q_DECLARE_FLAGS(ClientModes, ClientMode)


Q_DECLARE_OPERATORS_FOR_FLAGS(ActionScopes);
Q_DECLARE_OPERATORS_FOR_FLAGS(ClientModes);
Q_DECLARE_OPERATORS_FOR_FLAGS(ActionFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(ActionParameterTypes);

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

