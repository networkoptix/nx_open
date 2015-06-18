import QtQuick 2.4

pragma Singleton

QtObject {
    id: theme

    property color windowBackground
    property color windowText

    property color buttonBackground
    property color buttonAccentBackground
    property color buttonAttentionBackground
    property color buttonText

    property color inputBorder
    property color inputBorderActive
    property color inputBorderError
    property color inputText
    property color inputTextError
    property color inputPlaceholder

    property color listSectionText
    property color listText
    property color listSubText

    property color sessionItemBackground
    property color sessionItemBackgroundActive
    property color sessionItemActiveMark

    property color sideNavigationBackground
    property color sideNavigationCopyright
    property color sideNavigationSplitter
    property color sideNavigationSection

    property color attentionBackground

    function loadColors() {
        windowBackground        = colorTheme.color("window.background")
        windowText              = colorTheme.color("window.text")
        buttonBackground        = colorTheme.color("button.background")
        buttonAccentBackground  = colorTheme.color("button.accentBackground")
        buttonAttentionBackground = colorTheme.color("button.attentionBackground")
        buttonText              = colorTheme.color("button.text")
        inputBorder             = colorTheme.color("input.border")
        inputBorderActive       = colorTheme.color("input.borderActive")
        inputBorderError        = colorTheme.color("input.borderError")
        inputText               = colorTheme.color("input.text")
        inputTextError          = colorTheme.color("input.textError")
        inputPlaceholder        = colorTheme.color("input.placeholder")
        listSectionText         = colorTheme.color("list.sectionText")
        listText                = colorTheme.color("list.text")
        listSubText             = colorTheme.color("list.subText")
        sessionItemBackground   = colorTheme.color("sessionItem.background")
        sessionItemBackgroundActive = colorTheme.color("sessionItem.backgroundActive")
        sessionItemActiveMark   = colorTheme.color("sessionItem.activeMark")
        sideNavigationBackground = colorTheme.color("sideNavigation.background")
        sideNavigationCopyright = colorTheme.color("sideNavigation.copyright")
        sideNavigationSplitter  = colorTheme.color("sideNavigation.splitter")
        sideNavigationSection   = colorTheme.color("sideNavigation.section")
        attentionBackground     = colorTheme.color("other.attentionBackground")
    }

    function transparent(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    Component.onCompleted: loadColors()
}
