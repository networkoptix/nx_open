// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import Nx.Controls
import Nx.Dialogs

import nx.vms.client.core.analytics as Analytics
import nx.vms.client.desktop

import "taxonomy_utils.js" as TaxonomyUtils

ModalDialog
{
    id: dialog

    required property Analytics.StateView taxonomy
    required property LookupListModel model

    property Analytics.ObjectType objectType: taxonomy.objectTypeById(model.data.objectTypeId)
    property var entry: ({})

    acceptShortcutEnabled: addButton.enabled

    function updateAddButton()
    {
        addButton.enabled = Object.keys(entry).length > 0
    }

    minimumWidth: 450
    minimumHeight: 230
    maximumHeight: minimumHeight * 3
    title: qsTr("Add Entry")

    contentItem: Scrollable
    {
        id: table

        height: 132
        scrollView.ScrollBar.vertical.x: table.width - scrollView.ScrollBar.vertical.width + padding

        contentItem: Column
        {
            id: entries

            width: parent.width
            topPadding: padding / 2
            spacing: 8

            Repeater
            {
                model: dialog.model.attributeNames

                RowLayout
                {
                    id: rowLayout

                    Layout.fillWidth: true
                    spacing: 6

                    Label
                    {
                        Layout.preferredWidth: 70
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                        text: modelData
                    }

                    FocusScope
                    {
                        focus: true

                        onActiveFocusChanged:
                        {
                            if (activeFocus)
                                elementEditor.makeVisible()
                        }

                        implicitWidth: elementEditor.implicitWidth
                        implicitHeight: elementEditor.implicitHeight

                        LookupListElementEditor
                        {
                            id: elementEditor
                            Layout.preferredWidth: 340
                            objectType: dialog.objectType
                            attribute: TaxonomyUtils.findAttribute(objectType, modelData)

                            function makeVisible()
                            {
                                if (table.scrollView.contentHeight === -1)
                                {
                                    // Scroll view is not calculated content height yet.
                                    return
                                }

                                const scrollBar = table.scrollView.ScrollBar.vertical
                                const editorYOnScroll = rowLayout.y
                                let topYVisible = scrollBar.position * table.scrollView.contentHeight
                                let bottomYVisible = topYVisible + table.scrollView.height

                                if (editorYOnScroll < topYVisible)
                                {
                                    // Need to scroll down the page to fully show the editor.
                                    table.scrollContentY(topYVisible - editorYOnScroll)
                                }
                                else if (editorYOnScroll + height > bottomYVisible)
                                {
                                    // Need to scroll up the page to fully show the editor.
                                    table.scrollContentY(
                                        bottomYVisible - editorYOnScroll - height * 2 - rowLayout.spacing)
                                }
                            }

                            onValueChanged:
                            {
                                if (isDefinedValue())
                                    entry[modelData] = value
                                else
                                    delete entry[modelData]

                                updateAddButton()
                            }

                            Component.onCompleted:
                            {
                                if (index === 0)
                                    startEditing()
                            }
                        }
                    }
                }
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
            id: addButton

            text: qsTr("Add")
            width: Math.max(buttonBox.standardButton(DialogButtonBox.Cancel).width, implicitWidth)
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: false
            isAccentButton: true
        }
    }
}
