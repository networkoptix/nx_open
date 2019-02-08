import QtQuick 2.1;
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;

import "../rtu" as Rtu;
import "../base" as Base;
import "../expandable" as Expandable;
import "../../common" as Common;

Expandable.ExpandableItem
{
    id: thisComponent;

    property bool successfulSummary: true;
    property string caption;
    property var model;
    
    activeFocusOnTab: false;

    expanded: !successfulSummary;

    headerDelegate: Item
    {
        height: captionText.height + delimiter.height + delimiter.anchors.topMargin
            + (showChangesButton.visible ? showChangesButton.height + showChangesButton.anchors.topMargin : 0)
            + Common.SizeManager.spacing.large;
        
        anchors
        {
            left: parent.left;
            right: parent.right;
        }
        
        Base.Text
        {
            id: captionText;
            
            text: caption;
            font.pixelSize: Common.SizeManager.fontSizes.large;
        }
       
        Base.LineDelimiter
        {
            id: delimiter;
            anchors
            {
                top: captionText.bottom;
                topMargin: Common.SizeManager.spacing.medium;
            }
        }

        Base.Button
        {
            id: showChangesButton;
            
            height: Common.SizeManager.clickableSizes.medium;
            width: height * 4;   
            
            activeFocusOnTab: false;
            activeFocusOnPress: false;

            anchors
            {
                top: delimiter.bottom;
                topMargin: Common.SizeManager.spacing.medium;
            }
            
            visible: successfulSummary;
            text: qsTr("Changelog");
            
            onClicked: 
            {
                thisComponent.expanded = !thisComponent.expanded;
            }
        }
    }    

    areaDelegate: TableView
    {
        id: table;

        enabled: false;

        verticalScrollBarPolicy: Qt.ScrollBarAlwaysOff;

        height: 0;
        anchors
        {
            left: parent.left;
            right: parent.right;
        }

        activeFocusOnTab: false;

        backgroundVisible: false;
        headerVisible: false;
        contentHeader: Item {}
        contentFooter: Item {}
        style: ScrollViewStyle
        {
            frame: Item {}
        }

        selectionMode: SelectionMode.NoSelection;
        model: thisComponent.model;

        rowDelegate: Item
        {
            height: Common.SizeManager.clickableSizes.medium;
            Component.onCompleted: table.height += height;
        }

        section
        {
            property: "id";
            delegate: Rtu.SummaryTextDelegate
            {
                property string base: section;

                parentColumn: nameColumn;
                fontSize: Common.SizeManager.fontSizes.medium;
                text: base.substr(base.indexOf('}') + 1);

                Component.onCompleted: table.height += height;
            }
        }

        TableViewColumn
        {
            id: nameColumn;
            role: "name";
            width: 0;

            delegate: Rtu.SummaryTextDelegate
            {
                parentColumn: nameColumn;
                text: styleData.value;
            }
        }

        TableViewColumn
        {
            id: valueColumn;
            role: "value";
            width: 0;

            delegate: Rtu.SummaryTextDelegate
            {
                color: (table.model && table.model.isSuccessChangesModel ? "black" : "red");
                parentColumn: valueColumn;
                text: styleData.value;
            }
        }

        TableViewColumn
        {
            id: stateColumn;
            role: "success";
            width: Common.SizeManager.clickableSizes.base;

            delegate: Item
            {
                Image
                {
                    height: Common.SizeManager.clickableSizes.small;
                    width: Common.SizeManager.clickableSizes.small;
                    anchors.centerIn: parent;
                    source: table.model && table.model.isSuccessChangesModel
                        ? "qrc:/resources/changelog-ok.png"
                        : "qrc:/resources/changelog-fail.png";
                }
            }
        }

        TableViewColumn
        {
            id: reasonColumn;
            role: "reason";
            width: 0;

            delegate: Rtu.SummaryTextDelegate
            {
                parentColumn: reasonColumn;
                text: styleData.value;
            }
        }
    }
}
