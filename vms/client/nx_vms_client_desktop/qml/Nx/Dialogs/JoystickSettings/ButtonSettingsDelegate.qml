// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Models 1.0

import nx.vms.client.core
import nx.vms.client.desktop 1.0

Item
{
    id: control

    property JoystickButtonActionChoiceModel buttonActionChoiceModel: null
    property FilteredResourceProxyModel layoutModel: null

    property bool basicMode: true

    property int columnIndex: 0
    readonly property bool isLabelColumn: columnIndex === JoystickButtonSettingsModel.ButtonNameColumn
    readonly property bool isActionColumn:
        columnIndex === JoystickButtonSettingsModel.BaseActionColumn
        || columnIndex === JoystickButtonSettingsModel.ModifiedActionColumn
    readonly property bool isParameterColumn:
        columnIndex === JoystickButtonSettingsModel.BaseActionParameterColumn
        || columnIndex === JoystickButtonSettingsModel.ModifiedActionParameterColumn

    readonly property bool isOpenLayoutAction:
        model.actionParameterType === JoystickButtonSettingsModel.LayoutParameter

    readonly property bool isGoToPtzPositionAction:
        model.actionParameterType === JoystickButtonSettingsModel.HotkeyParameter

    // This property-function pairs are needed because their edits contain `model` property.

    readonly property int actionIndex:
        buttonActionChoiceModel && (isActionColumn || isParameterColumn)
            ? buttonActionChoiceModel.getRow(model.actionId)
            : actionIndex

    function setActionIndex(actionIndex)
    {
        const newActionId = buttonActionChoiceModel.data(
            buttonActionChoiceModel.index(actionIndex, 0),
            JoystickButtonActionChoiceModel.ActionIdRole)
        model.actionId = newActionId
    }

    readonly property int hotkeyIndex: isParameterColumn && isGoToPtzPositionAction
        ? model.hotkeyIndex
        : hotkeyIndex

    function setHotkeyIndex(hotkeyIndex)
    {
        model.hotkeyIndex = hotkeyIndex
    }

    implicitWidth: Math.max(loader.implicitWidth, 1)
    implicitHeight: loader.implicitHeight

    enabled: model.isEnabled

    Loader
    {
        id: loader

        anchors.verticalCenter: parent.verticalCenter

        sourceComponent:
        {
            if (control.isLabelColumn)
            {
                if (!model.isModifier || !control.basicMode)
                    return labelComponent
                else
                    return modifierLabelComponent
            }
            if (isActionColumn)
                return typeComboBoxComponent
            if (control.isParameterColumn && control.isGoToPtzPositionAction)
                return aimComboBoxComponent
            if (control.isParameterColumn && control.isOpenLayoutAction)
                return selectLayoutButtonComponent
            return null
        }

        Component
        {
            id: labelComponent

            Item
            {
                width: 82
                height: labelItem.height

                Label
                {
                    id: labelItem

                    x: 16
                    width: 66

                    maximumLineCount: 1
                    wrapMode: Text.WrapAnywhere
                    elide: Text.ElideRight
                    font.pixelSize: FontConfig.normal.pixelSize
                    color: model.buttonPressed
                        ? ColorTheme.colors.light10
                        : ColorTheme.colors.light4

                    text: model.display
                }
            }
        }

        Component
        {
            id: modifierLabelComponent

            Item
            {
                width: 82
                height: buttonLabelRect.height

                Rectangle
                {
                    id: buttonLabelRect

                    x: 16 - modifierLabelItem.x
                    width: MathUtils.bound(
                        0,
                        modifierLabelItem.contentWidth + modifierLabelItem.x * 2,
                        82)
                    height: modifierLabelItem.height + 4

                    radius: 2

                    border.color: modifierLabelItem.color
                    color: "transparent"

                    Label
                    {
                        id: modifierLabelItem

                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 66

                        maximumLineCount: 1
                        wrapMode: Text.WrapAnywhere
                        elide: Text.ElideRight
                        font.pixelSize: FontConfig.normal.pixelSize
                        color: model.buttonPressed
                            ? ColorTheme.darker(ColorTheme.colors.brand_core, 1)
                            : ColorTheme.colors.brand_core

                        text: model.display
                    }
                }
            }
        }

        Component
        {
            id: typeComboBoxComponent

            ActionComboBox
            {
                width: 230

                model: control.buttonActionChoiceModel

                currentIndex: control.actionIndex

                onActivated: setActionIndex(currentIndex)
            }
        }

        Component
        {
            id: aimComboBoxComponent

            ComboBox
            {
                width: 120

                model: hotkeysModel
                textRole: "display"

                currentIndex: hotkeyIndex === 0 ? 9 : hotkeyIndex - 1

                onActivated: control.setHotkeyIndex((currentIndex + 1) % 10)

                ListModel
                {
                    id: hotkeysModel

                    Component.onCompleted:
                    {
                        for (let i = 0; i < 10; ++i)
                            append({"display": qsTr("Hotkey %1").arg((i + 1) % 10)})
                    }
                }
            }
        }

        Component
        {
            id: selectLayoutButtonComponent

            Button
            {
                property var layoutInfo: ModelDataAccessor
                {
                    model: control.layoutModel

                    function getLayoutIndexByLogicalId(logicalId)
                    {
                        for (let row = 0; row < count; ++row)
                        {
                            if (getData(model.index(row, 0), "logicalId") === logicalId)
                                return row
                        }

                        return -1
                    }

                    function getLayoutIndexByUuid(uuid)
                    {
                        for (let row = 0; row < count; ++row)
                        {
                            if (getData(model.index(row, 0), "uuid") === uuid)
                                return row
                        }

                        return -1
                    }

                    function getLayoutName(layoutIndex)
                    {
                        return getData(model.index(layoutIndex, 0), "display")
                    }

                    function getLayoutLogicalId(layoutIndex)
                    {
                        return getData(model.index(layoutIndex, 0), "logicalId")
                    }

                    function getLayoutUuid(layoutIndex)
                    {
                        return getData(model.index(layoutIndex, 0), "uuid")
                    }
                }

                width: 180

                leftPadding: 8
                contentHAlignment: Qt.AlignLeft

                iconUrl: (model.iconKey && model.iconKey !== 0
                    && ("image://resource/" + model.iconKey)) || "image://svg/skin/tree/layout.svg"
                text: model.display

                onClicked: loader.sourceComponent = dialogComponent

                Loader
                {
                    id: loader

                    onLoaded: item.visible = true

                    Component
                    {
                        id: dialogComponent

                        SelectLayoutDialog
                        {
                            modality: Qt.WindowModal
                            layoutModel: control.layoutModel

                            onVisibleChanged:
                            {
                                if (!visible)
                                {
                                    loader.sourceComponent = null
                                    return
                                }

                                setSelectedLayoutIndex(model.layoutLogicalId !== 0
                                    ? layoutInfo.getLayoutIndexByLogicalId(model.layoutLogicalId)
                                    : layoutInfo.getLayoutIndexByUuid(model.layoutUuid))
                            }

                            onAccepted:
                            {
                                const layoutLogicalId = layoutInfo.
                                    getLayoutLogicalId(selectedLayoutIndex)

                                if (layoutLogicalId !== 0)
                                {
                                    model.layoutLogicalId = layoutLogicalId
                                }
                                else
                                {
                                    model.layoutUuid = layoutInfo.
                                        getLayoutUuid(selectedLayoutIndex)
                                }

                                reset()
                            }
                        }
                    }
                }
            }
        }
    }
}
