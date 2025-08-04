// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Mobile.Controls

import nx.vms.client.mobile

BottomSheet
{
    id: sheet

    property bool isAnalyticsItemMode: false
    property ShareBookmarkBackend backend

    signal showHowItWorks()

    title: isAnalyticsItemMode
        ? qsTr("New Bookmark")
        : qsTr("Sharing")

    function showSheet()
    {
        d.updating = true
        d.hasChanges = false

        const animationDuration = enableSharingToggle.animationDuration
        enableSharingToggle.animationDuration = 0

        backend.resetBookmarkData()

        enableSharingToggle.visible = backend.isShared || backend.isExpired

        let expiresInModel = [
            {text: qsTr("Expires in an hour"), expires: ShareBookmarkBackend.ExpiresIn.Hour},
            {text: qsTr("Expires in a day"), expires: ShareBookmarkBackend.ExpiresIn.Day},
            {text: qsTr("Expires in a month"), expires: ShareBookmarkBackend.ExpiresIn.Month},
            {text: qsTr("Never expires"), expires: ShareBookmarkBackend.ExpiresIn.Never}]

        let currentIndex = 1

        if (backend.isShared)
        {
            if (backend.isNeverExpires())
            {
                currentIndex = 3
            }
            else
            {
                currentIndex = 0
                expiresInModel.unshift(
                    {text: backend.expiresInText(), expires: backend.expirationTimeMs})
            }
        }

        expirationComboBox.model = expiresInModel
        expirationComboBox.currentIndex = currentIndex

        bookmarkDescriptionTextArea.text = backend.bookmarkDescription
        shareLinkOptionsText.text = qsTr("Shared link options")

        sharePasswordInput.resetState(/*hasSecretValue*/ !!backend.bookmarkDigest);

        enableSharingToggle.animationDuration = animationDuration

        d.updating = false

        open()
    }

    IconButton
    {
        id: infoButton

        visible: sheet.isAnalyticsItemMode
        parent: sheet.titleCustomArea
        padding: 0
        icon.source: "image://skin/20x20/Solid/info.svg?primary=light10"
        icon.width: 20
        icon.height: icon.width
        transform: Translate
        {
            x: (infoButton.width - infoButton.icon.width) / 2
        }

        onClicked:
        {
            sheet.close()
            sheet.showHowItWorks()
        }
    }

    TextInput
    {
        id: bookmarkNameInput

        backgroundMode: FieldBackground.Mode.Light

        width: parent.width

        visible: sheet.isAnalyticsItemMode

        labelText: qsTr("Name")
        text: backend.bookmarkName
    }

    TextArea
    {
        id: bookmarkDescriptionTextArea

        backgroundMode: FieldBackground.Mode.Light

        width: parent.width
        height: 156

        visible: sheet.isAnalyticsItemMode

        labelText: qsTr("Description")
    }

    TagView
    {
        id: bookmarkTags

        width: parent.width

        visible: sheet.isAnalyticsItemMode

        model: [BookmarkConstants.objectBasedTagName]
    }

    Text
    {
        id: shareLinkOptionsText

        visible: sheet.isAnalyticsItemMode
        width: parent.width

        color: ColorTheme.colors.light4
        font.weight: 500
        font.pixelSize: 18
        elide: Text.ElideRight
    }

    Switch
    {
        id: enableSharingToggle

        visible: !isAnalyticsItemMode
        parent: sheet.titleCustomArea

        toggleOnClick: false

        onClicked:
        {
            if (enableSharingToggle.checkState === Qt.PartiallyChecked)
                return

            const intermediateSuccess = backend.isShared
                ? d.stopSharing()
                : d.share()

            if (intermediateSuccess)
                enableSharingToggle.checkState = Qt.PartiallyChecked
        }

        Binding
        {
            target: enableSharingToggle
            property: "checkState"
            value: backend.isShared ? Qt.Checked : Qt.Unchecked
        }
    }

    ComboBox
    {
        id: expirationComboBox

        backgroundMode: FieldBackground.Mode.Light
        width: parent.width

        enabled: d.enabled

        popupTitle: qsTr("Select Expiration")
        labelText: qsTr("Expiration")

        textRole: "text"

        onCurrentIndexChanged: d.setHasChanges(true)
    }

    PasswordInput
    {
        id: sharePasswordInput

        backgroundMode: FieldBackground.Mode.Light
        width: parent.width

        enabled: d.enabled

        labelText: qsTr("Protect with Password (optional)")

        hasSecretValue: backend.bookmarkDigest.length

        onTextChanged: d.setHasChanges(true)
        onHasSecretValueChanged: d.setHasChanges(true)
    }

    ButtonBox
    {
        enabled: d.enabled

        property var kShareObjectButtonModel: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "share", type: Button.Type.Brand, text: qsTr("Create & Share")}
        ]

        property var kShareBookmarkModel: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "share", type: Button.Type.Brand, text: qsTr("Share Link")}
        ]

        property var kChangeShareParamsModel: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "share", type: Button.Type.Brand, text: qsTr("Save & Share")}
        ]

        model:
        {
            if (sheet.isAnalyticsItemMode)
                return kShareObjectButtonModel

            return d.hasChanges
                ? kChangeShareParamsModel
                : kShareBookmarkModel
        }

        onClicked:
            (id) =>
            {
                if (id == "share")
                {
                    if (backend.isExpired)
                        expirationComboBox.currentIndex = 1
                    d.share(/*showNativeShareSheet*/true)
                }

                sheet.close()
            }
    }

    NxObject
    {
        id: d

        property bool hasChanges: false
        property bool updating: false
        property bool enabled: isAnalyticsItemMode
            || !enableSharingToggle.visible
            || enableSharingToggle.checkState === Qt.Checked

        function share(showNativeShareSheet)
        {
            backend.bookmarkName = bookmarkNameInput.text
            backend.bookmarkDescription = bookmarkDescriptionTextArea.text

            const bookmarkPassword = sharePasswordInput.hasSecretValue
                ? backend.bookmarkDigest
                : sharePasswordInput.text

            return backend.share(
                expirationComboBox.model[expirationComboBox.currentIndex].expires,
                bookmarkPassword.trim(),
                showNativeShareSheet)
        }

        function stopSharing()
        {

            d.updating = true
            sharePasswordInput.text = ""
            sharePasswordInput.resetState(/*hasSecretValue*/false)
            d.updating = false

            return backend.stopSharing()
        }

        function setHasChanges(value)
        {
            if (!d.updating)
                d.hasChanges = value
        }
    }
}
