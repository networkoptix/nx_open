import QtQuick 2.1
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;


import "../base" as Base;
import "../../common" as Common;

GenericSettingsPanel
{
    id: thisComponent;

    property Component propertiesDelegate;

    property bool warned: (rtuContext.selection && (rtuContext.selection !== null)
        && (rtuContext.selection.count === 1) ? true : false);

    Component
    {
        id: maskingPanel;

        Item
        {
            height: warningSpacer.height + warningSpacer.anchors.topMargin + warningSpacer.anchors.bottomMargin;
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
                    topMargin: Common.SizeManager.spacing.large;
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

                    text: qsTr("Settings in this section are different for selected servers");
                    font.pointSize: Common.SizeManager.fontSizes.medium;
                }

                Row
                {
                    id: editButtonArea;

                    spacing: Common.SizeManager.spacing.small;

                    Base.Button
                    {
                        id: editAllButton;

                        height: Common.SizeManager.clickableSizes.medium;
                        width: height * 3;
                        text: qsTr("Edit all");

                        onClicked: { thisComponent.warned = true; }

                    }

                    Base.Text
                    {
                        id: warningText;

                        anchors.verticalCenter: parent.verticalCenter;

                        wrapMode: Text.Wrap;
                        text: qsTr("If you doesn't click this button nothing will be changed");
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

