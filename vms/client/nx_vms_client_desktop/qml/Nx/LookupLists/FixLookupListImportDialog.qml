// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx
import Nx.Core 1.0
import Nx.Controls
import Nx.Dialogs

import nx.vms.client.desktop
import nx.vms.client.desktop.analytics as Analytics

import "taxonomy_utils.js" as TaxonomyUtils

ModalDialog
{
    id: control

    required property Analytics.StateView taxonomy
    required property LookupListImportEntriesModel model
    property Analytics.ObjectType objectType: model.lookupListEntriesModel.listModel
        ? taxonomy.objectTypeById(model.lookupListEntriesModel.listModel.data.objectTypeId)
        : null;

    minimumWidth: 611
    minimumHeight: 400
    title: qsTr("Import List")

    signal fixupRequested

    onAccepted: fixupRequested()

    onRejected:
    {
        if(model)
            model.revertImport();
    }

    contentItem: Item
    {
        Rectangle
        {
            id: rectangle

            x: -16
            y: -16
            width: control.width
            height: 40
            color: ColorTheme.colors.dark8

            Text
            {
                anchors.verticalCenter: rectangle.verticalCenter
                leftPadding: 16
                text: qsTr("Some values could not be automatically matched. Please map them manually.")
                font.pixelSize: 14
                font.weight: Font.Medium
                color: ColorTheme.colors.light16
            }
        }

        Row
        {
            anchors.top: rectangle.bottom
            anchors.topMargin: 16
            width: control.width
            height: parent.height - y
            spacing: 10

            Flickable
            {
                id: table

                width: control.contentItem.width -
                    (scrollBar.visible ? scrollBar.width + parent.spacing : 0)
                height: parent.height
                clip: true
                flickableDirection: Flickable.VerticalFlick
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: width
                contentHeight: fixups.height

                ScrollBar.vertical: scrollBar

                Column
                {
                    id: fixups

                    property var fixupData: control.model.fixupData

                    topPadding: padding / 2
                    spacing: 16
                    width: parent.width

                    Repeater
                    {
                        model: Object.keys(fixups.fixupData)

                        ColumnLayout
                        {
                            id: repeaterByAttribute

                            property var attributeName: modelData
                            property var incorrectWords: Object.keys(fixups.
                                fixupData[attributeName].incorrectToCorrectWord)

                            width: parent.width
                            spacing: 3

                            Text
                            {
                                id: attributeHeader

                                text: repeaterByAttribute.attributeName
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: ColorTheme.colors.light4
                            }

                            Rectangle
                            {
                                height: 1
                                Layout.bottomMargin: 20
                                Layout.fillWidth: true
                                color: ColorTheme.colors.dark12
                            }

                            Repeater
                            {
                                model: repeaterByAttribute.incorrectWords

                                RowLayout
                                {
                                    spacing: 8

                                    Label
                                    {
                                        Layout.preferredWidth: 120
                                        horizontalAlignment: Text.AlignRight
                                        text: modelData
                                    }

                                    ColoredImage
                                    {
                                        sourceSize: Qt.size(20, 20)
                                        sourcePath: "24x24/Outline/arrow_right_2px.svg"
                                        primaryColor: ColorTheme.colors.light16
                                    }

                                    LookupListElementEditor
                                    {
                                        Layout.fillWidth: true
                                        objectType: control.objectType
                                        attribute: TaxonomyUtils.findAttribute(objectType,
                                            repeaterByAttribute.attributeName)

                                        onValueChanged:
                                        {
                                            if (isDefinedValue())
                                            {
                                                control.model.addFixForWord(repeaterByAttribute.attributeName,
                                                    modelData, value);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ScrollBar
            {
                id: scrollBar

                anchors.top: table.top
                anchors.bottom: table.bottom
                visible: table.height < table.contentHeight
                policy: ScrollBar.AlwaysOn
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttonBox

        buttonLayout: DialogButtonBox.KdeLayout
        standardButtons: DialogButtonBox.Cancel

        Button
        {
            id: updateButon

            width: Math.max(buttonBox.standardButton(DialogButtonBox.Cancel).width, implicitWidth)
            text: qsTr("Finish")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: true
            isAccentButton: true
        }
    }
}
