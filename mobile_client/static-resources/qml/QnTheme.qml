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
    property color inputText
    property color inputPlaceholder
    property color inputBorderError
    property color inputBorderActiveError
    property color inputTextError
    property color inputPlaceholderError

    property color listSectionText
    property color listText
    property color listSubText
    property color listSeparator

    property color sessionItemBackground
    property color sessionItemBackgroundActive
    property color sessionItemActiveMark

    property color sideNavigationBackground
    property color sideNavigationCopyright
    property color sideNavigationSplitter
    property color sideNavigationSection

    property color popupMenuBackground
    property color popupMenuText

    property color dialogBackground
    property color dialogText
    property color dialogTitleText
    property color dialogSelectedBackground
    property color dialogSelectedText

    property color cameraText
    property color cameraOfflineText
    property color cameraRecordingIndicator
    property color cameraBorder

    property color timelineText
    property color timelineChunk

    property color calendarBackground
    property color calendarText
    property color calendarArchiveIndicator
    property color calendarDayName
    property color calendarHolidayName
    property color calendarDisabledText
    property color calendarSelectedBackground
    property color calendarSelectedText

    property color attentionBackground
    property color loadingText

    function loadColors() {
        windowBackground        = colorTheme.color("window.background")
        windowText              = colorTheme.color("window.text")
        buttonBackground        = colorTheme.color("button.background")
        buttonAccentBackground  = colorTheme.color("button.accentBackground")
        buttonAttentionBackground = colorTheme.color("button.attentionBackground")
        buttonText              = colorTheme.color("button.text")
        inputBorder             = colorTheme.color("input.border")
        inputBorderActive       = colorTheme.color("input.borderActive")
        inputText               = colorTheme.color("input.text")
        inputPlaceholder        = colorTheme.color("input.placeholder")
        inputBorderError        = colorTheme.color("input.borderError")
        inputBorderActiveError  = colorTheme.color("input.borderActiveError")
        inputTextError          = colorTheme.color("input.textError")
        inputPlaceholderError   = colorTheme.color("input.placeholderError")
        listSectionText         = colorTheme.color("list.sectionText")
        listText                = colorTheme.color("list.text")
        listSubText             = colorTheme.color("list.subText")
        listSeparator           = colorTheme.color("list.separator")
        sessionItemBackground   = colorTheme.color("sessionItem.background")
        sessionItemBackgroundActive = colorTheme.color("sessionItem.backgroundActive")
        sessionItemActiveMark   = colorTheme.color("sessionItem.activeMark")
        sideNavigationBackground = colorTheme.color("sideNavigation.background")
        sideNavigationCopyright = colorTheme.color("sideNavigation.copyright")
        sideNavigationSplitter  = colorTheme.color("sideNavigation.splitter")
        sideNavigationSection   = colorTheme.color("sideNavigation.section")
        popupMenuBackground     = colorTheme.color("popupMenu.background")
        popupMenuText           = colorTheme.color("popupMenu.text")
        dialogBackground        = colorTheme.color("dialog.background")
        dialogText              = colorTheme.color("dialog.text")
        dialogTitleText         = colorTheme.color("dialog.titleText")
        dialogSelectedBackground = colorTheme.color("dialog.selectedBackground")
        dialogSelectedText      = colorTheme.color("dialog.selectedText")
        cameraText              = colorTheme.color("cameras.text")
        cameraOfflineText       = colorTheme.color("cameras.offlineText")
        cameraRecordingIndicator = colorTheme.color("cameras.recordingIndicator")
        cameraBorder            = colorTheme.color("cameras.border")
        timelineText            = colorTheme.color("timeline.text")
        timelineChunk           = colorTheme.color("timeline.chunk")
        calendarBackground      = colorTheme.color("calendar.background")
        calendarText            = colorTheme.color("calendar.text")
        calendarArchiveIndicator = colorTheme.color("calendar.archiveIndicator")
        calendarDayName         = colorTheme.color("calendar.dayName")
        calendarHolidayName     = colorTheme.color("calendar.holidayName")
        calendarDisabledText    = colorTheme.color("calendar.disabledText")
        calendarSelectedBackground = colorTheme.color("calendar.selectedBackground")
        calendarSelectedText    = colorTheme.color("calendar.selectedText")
        attentionBackground     = colorTheme.color("other.attentionBackground")
        loadingText             = colorTheme.color("other.loadingText")
    }

    function transparent(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    Component.onCompleted: loadColors()
}
