/* Bespin widget style for Qt4
   Copyright (C) 2007 Thomas Luebking <thomas.luebking@web.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#include <QEvent>
#include <QFrame>
#include <QMenuBar>
#include "blib/colors.h"
#include "bespin.h"
#ifdef Q_WS_X11
#ifdef BESPIN_MACMENU
#include "macmenu.h"
#endif
#endif
#include "makros.h"

#if  0 // QT_VERSION >= 0x040500 - no. see below!
#include <QStyleOptionTab>
#include <QTabBar>
#include <QTabWidget>
#endif

using namespace Bespin;

static const QStyle::StyleHint SH_KCustomStyleElement = (QStyle::StyleHint)0xff000001;

int Style::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData ) const
{
    switch (hint)
    {
    case SH_EtchDisabledText:
    case SH_DitherDisabledText: //
        return false; // Both look CRAP! (win has etching)
#ifdef QT3_SUPPORT
    case SH_GUIStyle:
        return Qt::MacStyle;
#endif
    case SH_ScrollBar_MiddleClickAbsolutePosition:
        return true; // support middle click jumping
    case SH_ScrollBar_LeftClickAbsolutePosition:
        return false; // annoying
    case SH_ScrollBar_ScrollWhenPointerLeavesControl:
        return true; // UIs are no ego shooters...
    case SH_TabBar_Alignment:
#if  0 // QT_VERSION >= 0x040500 DON'T TRY AGAIN, other thomas - it looks crap :-(
//         qDebug() << option << widget;
        if (const QStyleOptionTabV3 *tab = qstyleoption_cast<const QStyleOptionTabV3*>(option))
            return (tab->documentMode ? Qt::AlignLeft : Qt::AlignCenter);
        if (const QTabBar *tabBar = qobject_cast<const QTabBar*>(widget))
            return (tabBar->documentMode() ? Qt::AlignLeft : Qt::AlignCenter);
        if (const QTabWidget *tabWidget = qobject_cast<const QTabWidget*>(widget))
            return (tabWidget->documentMode() ? Qt::AlignLeft : Qt::AlignCenter);
#else
        return Qt::AlignLeft;
#endif
    case SH_Header_ArrowAlignment:
        return Qt::AlignLeft; // we move it to text center though...
    case SH_Slider_SnapToValue:
        return true; // yes, better then snapping after releasing the slider!
    case SH_Slider_SloppyKeyEvents:
        return true; // allow left/right & top/bottom on both orientations
    case SH_ProgressDialog_CenterCancelButton:
        return false; // looks strange
    case SH_ProgressDialog_TextLabelAlignment:
        return Qt::AlignCenter; // is this used anywhere?
    case SH_PrintDialog_RightAlignButtons:
        return true; // ok/cancel just belong there
    case SH_MainWindow_SpaceBelowMenuBar:
#ifdef Q_WS_X11
#ifdef BESPIN_MACMENU
        if (MacMenu::isActive())
        if (const QMenuBar *menubar = qobject_cast<const QMenuBar*>(widget))
        if (MacMenu::manages(menubar))
//         if (menubar->height() == 0)
        if (!menubar->actions().isEmpty())
        {   // we trick menubars if we use macmenus - hehehe...
            // NOTICE the final result NEEDS to be > "0" (i.e. "1") to avoid side effects...
            return -menubar->actionGeometry(menubar->actions().first()).height() + !config.UNO.title;
        }
#endif
#endif
        return 0; // no space between menus and docks (we need padding rather than margin)
///    case SH_FontDialog_SelectAssociatedText: // Select the text in the line edit, or when selecting an item from the listbox, or when the line edit receives focus, as done on Windows.
///    case SH_Menu_AllowActiveAndDisabled: // Allows disabled menu items to be active.
    case SH_Menu_SpaceActivatesItem:
        return true; // yes
    case SH_Menu_SubMenuPopupDelay:
        return config.menu.delay;
//         return 96; // motif value - don't have time...
    case SH_Menu_Scrollable:
        return true; // better scroll than fold around covering the desktop!
    case SH_Menu_SloppySubMenus:
        return true; // don't hide submenus immediately please
    case SH_ScrollView_FrameOnlyAroundContents: // YES - period.
        return (!(widget && widget->inherits("QComboBoxListView")));
    case SH_MenuBar_AltKeyNavigation:
        return true;
    case SH_ComboBox_ListMouseTracking:
    case SH_Menu_MouseTracking:
    case SH_MenuBar_MouseTracking:
        return true; // feedback to the user please!
    case SH_Menu_FillScreenWithScroll:
        return false; // don't trash the desktop
    case SH_ItemView_ChangeHighlightOnFocus:
        return true; //config.fadeInactive; // Gray out selected items when losing focus.
///    case SH_Widget_ShareActivation: // Turn on sharing activation with floating modeless dialogs.
    case SH_TabBar_SelectMouseType:
        // NOTICE WORKAROUND! MouseButtonRelease causes trouble with konqueror's doubleclicking
        return appType == Konqueror ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease; // =)
#ifdef QT3_SUPPORT
    case SH_Q3ListViewExpand_SelectMouseType:
        return QEvent::MouseButtonPress;
#endif
    case SH_TabBar_PreferNoArrows:
        return false; // the can grow horribly big...
    case SH_ComboBox_Popup: // Allows popups as a combobox drop-down menu.
//         if (const QComboBox *cmb = qobject_cast<const QComboBox *>(widget))
//         if (!cmb->isEditable())
//             return cmb->count() < 11; // maybe depend on item count?!
        return false;
    case SH_ComboBox_PopupFrameStyle:
        return QFrame::StyledPanel | QFrame::Plain;
///    case SH_Workspace_FillSpaceOnMaximize: // The workspace should maximize the client area.
    case SH_TitleBar_NoBorder:
        return true; // The title bar has no border.
    case SH_ScrollBar_StopMouseOverSlider:
        return false; // Stops auto-repeat when the slider reaches the mouse position. (hähh?)
    case SH_BlinkCursorWhenTextSelected:
//         false; // that's annoying but ...
        return true; // ... great, this is now completely ignored anyway winblows ftw :-(
    case SH_RichText_FullWidthSelection:
        return true;
    case SH_GroupBox_TextLabelVerticalAlignment:
        return Qt::AlignTop; // we've no halfheight frame
    case SH_GroupBox_TextLabelColor:
        return QPalette::WindowText;
///    case SH_DialogButtons_DefaultButton: // Which button gets the default status in a dialog's button widget.
    case SH_DialogButtonLayout:
        return config.dialogBtnLayout;
    case SH_DialogButtonBox_ButtonsHaveIcons:
        return false; // no way
    case SH_ToolBox_SelectedPageTitleBold:
        return true; // yes please
    case SH_LineEdit_PasswordCharacter:
    { // configurable...
        const QFontMetrics &fm = option ? option->fontMetrics :
                                (widget ? widget->fontMetrics() : QFontMetrics(QFont()));
        if (fm.inFont(QChar(config.input.pwEchoChar)))
            return config.input.pwEchoChar;
        return '*';
        }
    case SH_Table_GridLineColor:
        if (option)
            return Colors::mid(FCOLOR(Base), FCOLOR(Text),6,1).rgb();
        return -1;
    case SH_UnderlineShortcut:
        return true; // means the alt+<x> menu thing, we handle global setting throught BESPIN_MNEMONIC
    case SH_SpinBox_AnimateButton:
        return true; // feedback to the user, please
    case SH_SpinBox_ClickAutoRepeatRate:
        return 150;
    case SH_SpinBox_KeyPressAutoRepeatRate:
        return 75;
    case SH_ToolTipLabel_Opacity:
        return 204; // ~ 80%
    case SH_DrawMenuBarSeparator:
        return false; // NO WAY!!!
    case SH_TitleBar_ModifyNotification:
        return true; // add '*' to window title (or however WM manages it)
    case SH_Button_FocusPolicy:
        return Qt::StrongFocus;
    case SH_MenuBar_DismissOnSecondClick:
        return true; // simple close popups
    case SH_MessageBox_UseBorderForButtonSpacing:
        return false; // hähh?
    case SH_TitleBar_AutoRaise:
        return true; // hover titlebar buttons in MDI
    case SH_ToolButton_PopupDelay:
        return 150; // everyone can do a click in 150ms - yesno?
///    case SH_FocusFrame_Mask: // The mask of the focus frame.
    case SH_RubberBand_Mask: // The mask of the rubber band.
        return false; // we have an opaque one
///    case SH_WindowFrame_Mask: // The mask of the window frame.
    case SH_SpinControls_DisableOnBounds:
        return true; // yeah - don't trick the user
    case SH_Dial_BackgroundRole:
        return QPalette::Window;
    case SH_ComboBox_LayoutDirection:
        return config.leftHanded ? Qt::RightToLeft : Qt::LeftToRight;
    case SH_ItemView_ArrowKeysNavigateIntoChildren:
        return true;
    case SH_ItemView_EllipsisLocation:
        return Qt::AlignTrailing;
    case SH_ItemView_ShowDecorationSelected:
        return true; // full width selection
    case SH_ItemView_ActivateItemOnSingleClick:
        return config.macStyle;
    case SH_WizardStyle:
        return config.macStyle ? 2 : 1; // QWizard::MacStyle / QWizard::ModernStyle

    case SH_FormLayoutWrapPolicy:
        return 0; //  QFormLayout::RowWrapPolicy
    case SH_FormLayoutFieldGrowthPolicy:
        return config.macStyle ? 0 : 1; // QFormLayout::FieldsStayAtSizeHint / ExpandingFieldsGrow / AllNonFixedFieldsGrow
    case SH_FormLayoutFormAlignment:
        return (Qt::AlignHCenter | Qt::AlignTop); // (Qt::AlignLeft | Qt::AlignTop)
    case SH_FormLayoutLabelAlignment:
        return Qt::AlignRight;
    case SH_ItemView_PaintAlternatingRowColorsForEmptyArea:
        return true;

    case SH_KCustomStyleElement:
    {
        if (!widget)
            return 0;
        int id = elementId(widget->objectName());
        if (!id)
            qWarning("Unsupported KCustomStyleElement requested: %s", widget->objectName().toLatin1().data());
        return id;
    }
    default:
        return QCommonStyle::styleHint(hint, option, widget, returnData);
    } // switch
}
