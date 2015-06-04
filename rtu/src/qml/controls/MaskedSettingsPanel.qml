import QtQuick 2.3
import QtQuick.Controls 1.3;
import QtQuick.Controls.Styles 1.3;


import "../controls";

GenericSettingsPanel
{
    id: thisComponent;

    property Component propertiesDelegate;

    property bool warned: (rtuContext.selectedServersCount === 1 ? true : false);

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

            Column
            {
                id: warningSpacer;

                spacing: SizeManager.spacing.base;
                anchors
                {
                    left: parent.left;
                    right: parent.right;
                    top: parent.top;

                    leftMargin: SizeManager.spacing.base;
                    topMargin: SizeManager.spacing.large;
                    bottomMargin: warningSpacer.anchors.topMargin;
                }

                Text
                {
                    id: caption;

                    anchors
                    {
                        left: parent.left;
                        right: parent.right;

                        leftMargin: SizeManager.spacing.base;
                    }

                    text: qsTr("Settings in this section are different for selected servers");
                    font.pointSize: SizeManager.fontSizes.medium;
                }

                Row
                {
                    id: editButtonArea;

                    spacing: SizeManager.spacing.small;

                    Button
                    {
                        id: editAllButton;

                        height: SizeManager.clickableSizes.medium;
                        width: height * 3;
                        text: qsTr("Edit all");

                        style: ButtonStyle
                        {
                            label: Text
                            {
                                renderType: Text.NativeRendering;
                                verticalAlignment: Text.AlignVCenter;
                                horizontalAlignment: Text.AlignHCenter;
                                font.pointSize: SizeManager.fontSizes.medium;
                                text: control.text;
                            }
                        }
                
                        onClicked: { thisComponent.warned = true; }

                    }

                    Text
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

