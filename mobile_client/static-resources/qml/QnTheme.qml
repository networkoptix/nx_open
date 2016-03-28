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
    property color buttonAccentText

    property color inputBorder
    property color inputBorderActive
    property color inputText
    property color inputPlaceholder
    property color inputBorderError
    property color inputBorderActiveError
    property color inputTextError
    property color inputPlaceholderError

    property color checkBoxText
    property color checkBoxBoxOn
    property color checkBoxBoxOff

    property color listSectionText
    property color listText
    property color listSubText
    property color listSeparator
    property color listDisabledText
    property color listSelectionBorder

    property color sessionItemBackground
    property color sessionItemBackgroundActive
    property color sessionItemBackgroundIncompatible
    property color sessionItemActiveMark
    property color sessionItemIncompatibleMark

    property color sideNavigationBackground
    property color sideNavigationSplitter
    property color sideNavigationSection

    property color popupMenuBackground
    property color popupMenuText

    property color dialogBackground
    property color dialogClickBackground
    property color dialogText
    property color dialogTitleText
    property color dialogSelectedBackground
    property color dialogSelectedText
    property color dialogButtonSplitter

    property color cameraBackground
    property color cameraText
    property color cameraOfflineText
    property color cameraRecordingIndicator
    property color cameraOfflineBackground
    property color cameraHiddenText
    property color cameraHiddenBackground
    property color cameraDummyBorder
    property color cameraShowButtonBackground

    property color toastBackground
    property color toastText

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
    property color calendarSplitter

    property color attentionBackground
    property color loadingText
    property color preloaderDot
    property color preloaderCircle
    property color scrollIndicator
    property color playPause
    property color playPauseBackground
    property color navigationPanelBackground
    property color offlineDimmer
    property color cameraStatus

    function loadColors() {
        windowBackground        = colorTheme.color("window.background")
        windowText              = colorTheme.color("window.text")
        buttonBackground        = colorTheme.color("button.background")
        buttonAccentBackground  = colorTheme.color("button.accentBackground")
        buttonAttentionBackground = colorTheme.color("button.attentionBackground")
        buttonText              = colorTheme.color("button.text")
        buttonAccentText        = colorTheme.color("button.accentText")
        inputBorder             = colorTheme.color("input.border")
        inputBorderActive       = colorTheme.color("input.borderActive")
        inputText               = colorTheme.color("input.text")
        inputPlaceholder        = colorTheme.color("input.placeholder")
        inputBorderError        = colorTheme.color("input.borderError")
        inputBorderActiveError  = colorTheme.color("input.borderActiveError")
        inputTextError          = colorTheme.color("input.textError")
        inputPlaceholderError   = colorTheme.color("input.placeholderError")
        checkBoxText            = colorTheme.color("checkBox.text")
        checkBoxBoxOff          = colorTheme.color("checkBox.boxOff")
        checkBoxBoxOn           = colorTheme.color("checkBox.boxOn")
        listSectionText         = colorTheme.color("list.sectionText")
        listText                = colorTheme.color("list.text")
        listSubText             = colorTheme.color("list.subText")
        listSeparator           = colorTheme.color("list.separator")
        listDisabledText        = colorTheme.color("list.disabledText")
        listSelectionBorder     = colorTheme.color("list.selectionBorder")
        sessionItemBackground   = colorTheme.color("sessionItem.background")
        sessionItemBackgroundActive = colorTheme.color("sessionItem.backgroundActive")
        sessionItemBackgroundIncompatible = colorTheme.color("sessionItem.backgroundIncompatible")
        sessionItemActiveMark   = colorTheme.color("sessionItem.activeMark")
        sessionItemIncompatibleMark = colorTheme.color("sessionItem.incompatibleMark")
        sideNavigationBackground = colorTheme.color("sideNavigation.background")
        sideNavigationSplitter  = colorTheme.color("sideNavigation.splitter")
        sideNavigationSection   = colorTheme.color("sideNavigation.section")
        popupMenuBackground     = colorTheme.color("popupMenu.background")
        popupMenuText           = colorTheme.color("popupMenu.text")
        dialogBackground        = colorTheme.color("dialog.background")
        dialogClickBackground   = colorTheme.color("dialog.clickBackground")
        dialogText              = colorTheme.color("dialog.text")
        dialogTitleText         = colorTheme.color("dialog.titleText")
        dialogSelectedBackground = colorTheme.color("dialog.selectedBackground")
        dialogSelectedText      = colorTheme.color("dialog.selectedText")
        dialogButtonSplitter    = colorTheme.color("dialog.buttonSplitter")
        cameraBackground        = colorTheme.color("cameras.background")
        cameraText              = colorTheme.color("cameras.text")
        cameraOfflineText       = colorTheme.color("cameras.offlineText")
        cameraRecordingIndicator = colorTheme.color("cameras.recordingIndicator")
        cameraOfflineBackground = colorTheme.color("cameras.offlineBackground")
        cameraHiddenText        = colorTheme.color("cameras.hiddenText")
        cameraHiddenBackground  = colorTheme.color("cameras.hiddenBackground")
        cameraDummyBorder       = colorTheme.color("cameras.dummyBorder")
        cameraShowButtonBackground = colorTheme.color("cameras.showButtonBackground")
        toastBackground         = colorTheme.color("toast.background")
        toastText               = colorTheme.color("toast.text")
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
        calendarSplitter        = colorTheme.color("calendar.splitter")
        attentionBackground     = colorTheme.color("other.attentionBackground")
        loadingText             = colorTheme.color("other.loadingText")
        preloaderDot            = colorTheme.color("other.preloaderDot")
        preloaderCircle         = colorTheme.color("other.preloaderCircle")
        scrollIndicator         = colorTheme.color("other.scrollIndicator")
        playPause               = colorTheme.color("other.playPause")
        playPauseBackground     = colorTheme.color("other.playPauseBackground")
        navigationPanelBackground = colorTheme.color("other.navigationPanelBackground")
        offlineDimmer           = colorTheme.color("other.offlineDimmer")
        cameraStatus            = colorTheme.color("other.cameraStatus")
    }

    function transparent(color, opacity) {
        return Qt.rgba(color.r, color.g, color.b, opacity)
    }

    Component.onCompleted: loadColors()
}
