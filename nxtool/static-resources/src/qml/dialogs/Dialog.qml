import QtQuick 2.0;
import QtQuick.Window 2.0;
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


    signal buttonClicked(int id);

    title: "";
    flags: Qt.WindowTitleHint | Qt.MSWindowsFixedSizeDialogHint;
    modality: Qt.WindowModal;

    height: loader.height + buttonsPanel.height;
    width: Math.max(buttonsPanel.width, loader.width );

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
        anchors.right: parent.right;

        onButtonClicked:
        {
            thisComponent.close();
            thisComponent.buttonClicked(id);
        }
    }
}
