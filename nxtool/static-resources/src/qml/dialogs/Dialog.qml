import QtQuick 2.3;
import QtQuick.Window 2.1;
import QtQuick.Controls 1.2;

import "../common" as Common;
import "../controls/base" as Base;
import "../controls/rtu" as Rtu;

Window
{
    id: thisComponent;

    property int buttons: 0;
    property int styledButtons: 0;

    property Component areaDelegate;
    property alias area: loader.item;
    property alias dontShowText: dontShowNextTimeControl.text;

    signal buttonClicked(int id);
    signal dontShowNextTimeChanged(bool dontShow);

    title: "";
    flags: Qt.WindowTitleHint | Qt.MSWindowsFixedSizeDialogHint;
    modality: Qt.WindowModal;

    height: loader.height + buttonsPanel.height;
    width: Math.max(buttonsPanel.width, loader.width);

    Loader
    {
        id: loader;

        sourceComponent: areaDelegate;
    }

    Base.ButtonsPanel
    {
        id: buttonsPanel;

        buttons: thisComponent.buttons;
        styled: thisComponent.styledButtons;

        y: thisComponent.height - height;
        x: thisComponent.width - width;

        onButtonClicked:
        {
            thisComponent.close();
            thisComponent.buttonClicked(id);
        }

    }

    Base.CheckBox
    {
        id: dontShowNextTimeControl;

        anchors
        {
            verticalCenter: buttonsPanel.verticalCenter;
            left: parent.left;
            leftMargin: Common.SizeManager.spacing.base;
        }

        visible: text.length;

        initialCheckedState: Qt.Unchecked;
        onCheckedChanged: { thisComponent.dontShowNextTimeChanged(dontShowNextTimeControl.checked); }
    }
}
