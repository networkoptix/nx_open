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

    title: isAnalyticsItemMode
        ? qsTr("New Bookmark")
        : d.kShareOptionTextString

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
        text: backend.bookmarkDescription
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

        text: d.kShareOptionTextString

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
    }

    ComboBox
    {
        id: lifetimeComboBox

        backgroundMode: FieldBackground.Mode.Light
        width: parent.width

        enabled: enableSharingToggle.checkState === Qt.Checked

        labelText: qsTr("Lifetime")

        textRole: "text"

        currentIndex: 0
    }

    PasswordInput
    {
        id: sharePasswordInput

        backgroundMode: FieldBackground.Mode.Light
        width: parent.width

        enabled: enableSharingToggle.checkState === Qt.Checked

        labelText: qsTr("Protect with Password (optional)")
        secretValue: backend.bookmarkDigest
    }

    ButtonBox
    {
        property var kShareObjectButtonModel: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "share", type: Button.Type.Brand, text: qsTr("Create & Share")}
        ]

        property var kSaveAndShareButtonModel: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "share", type: Button.Type.Brand, text: qsTr("Save & Share")}
        ]

        property var kStopSharingButtonModel: [
            {id: "cancel", type: Button.Type.LightInterface, text: qsTr("Cancel")},
            {id: "stop", type: Button.Type.Brand, text: qsTr("Stop Sharing")}
        ]

        model:
        {
            if (sheet.isAnalyticsItemMode)
                return kShareObjectButtonModel

            return enableSharingToggle.checkState === Qt.Checked
                ? kSaveAndShareButtonModel
                : kStopSharingButtonModel
        }

        onClicked:
            (id) =>
            {
                switch (id)
                {
                    case "share":
                        backend.bookmarkName = bookmarkNameInput.text
                        backend.bookmarkDescription = bookmarkDescriptionTextArea.text

                        const bookmarkPassword = sharePasswordInput.secretValue
                            ? sharePasswordInput.secretValue
                            : sharePasswordInput.text
                        backend.share(
                            lifetimeComboBox.model[lifetimeComboBox.currentIndex].expires,
                            bookmarkPassword.trim())
                        break;
                    case "stop":
                        backend.stopSharing()
                        break;
                }

                sheet.close()
            }
    }

    onOpened:
    {
        const currentAnimationDuration = enableSharingToggle.animationDuration
        enableSharingToggle.animationDuration = 0
        enableSharingToggle.checkState = Qt.Checked
        enableSharingToggle.animationDuration = currentAnimationDuration

        let expiresInModel = [
            {text: qsTr("Expires in an hour"), expires: ShareBookmarkBackend.ExpiresIn.Hour},
            {text: qsTr("Expires in a day"), expires: ShareBookmarkBackend.ExpiresIn.Day},
            {text: qsTr("Expires in a month"), expires: ShareBookmarkBackend.ExpiresIn.Month},
            {text: qsTr("Never expires"), expires: ShareBookmarkBackend.ExpiresIn.Never}]

        let currentIndex = 0

        if (backend.isSharedNow())
        {
            if (backend.isNeverExpires())
            {
                currentIndex = 3
            }
            else
            {
                expiresInModel.unshift(
                    {text: backend.expiresInText(), expires: backend.expirationTimeMs})
            }
        }

        lifetimeComboBox.model = expiresInModel
        lifetimeComboBox.currentIndex = currentIndex

        sharePasswordInput.secretValue = backend.bookmarkDigest;
        sharePasswordInput.resetState()
    }

    NxObject
    {
        id: d

        readonly property string kShareOptionTextString: qsTr("Shared link options")
    }
}
