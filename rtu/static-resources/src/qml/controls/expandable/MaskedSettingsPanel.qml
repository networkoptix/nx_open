import QtQuick 2.1
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;


import "../base" as Base;
import "../../common" as Common;

GenericSettingsPanel
{
    id: thisComponent;

    property Component propertiesDelegate;
    property var maskedArea: area.item;
    
    property bool warned: (rtuContext.selection && (rtuContext.selection !== null)
        && (rtuContext.selection.count === 1) ? true : false);

    Component
    {
        id: maskingPanel;

        Item
        {
            height: warningSpacer.height + warningSpacer.anchors.bottomMargin;
            anchors
            {
                left: ( parent ? parent.left : undefined );
                right: ( parent ? parent.right : undefined );
                top: ( parent ? parent.top : undefined );
            }

            Base.Column
            {
                id: warningSpacer;

                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    top: parent.top;

                    leftMargin: Common.SizeManager.spacing.base;
                    bottomMargin: warningSpacer.anchors.topMargin;
                }

                Text
                {
                    id: caption;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;

                        leftMargin: Common.SizeManager.spacing.base;
                    }

                    wrapMode: Text.Wrap;
                    font.pixelSize: Common.SizeManager.fontSizes.medium;
                    text: qsTr("Settings in this partition are different for selected servers.");
                }

                Row
                {
                    id: editButtonArea;

                    spacing: Common.SizeManager.spacing.medium;

                    Base.Button
                    {
                        id: editAllButton;

                        height: Common.SizeManager.clickableSizes.medium;
                        width: height * 4;
                        text: qsTr("Edit all");

                        onClicked: { thisComponent.warned = true; }

                    }

                    Base.Text
                    {
                        id: warningText;

                        anchors.verticalCenter: parent.verticalCenter;
                        
                        color: "#666666";
                        wrapMode: Text.Wrap;
                        font.pixelSize: Common.SizeManager.fontSizes.base;
                        text: qsTr("If you doesn't click this button,\nnothing will changed");
                    }
                }
            }
        }
    }

    areaDelegate: Loader
    {
        id: areaDelegateLoader;
        
        height: (item ? item.height : 0);
        sourceComponent: (thisComponent.warned ? propertiesDelegate : maskingPanel);
    }
}

