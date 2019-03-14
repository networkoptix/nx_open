import QtQuick 2.1
import QtQuick.Controls 1.1;
import QtQuick.Controls.Styles 1.1;


import "../base" as Base;
import "../../common" as Common;

GenericSettingsPanel
{
    id: thisComponent;

    property bool editBtnPrevTabIsSelf: false;

    property Component propertiesDelegate;
    property var maskedArea: area.item;
    
    // TODO: #ynikitenkov  (rtuContext.selection !== null) should not be required  here
    property bool extraWarned: false;
    property bool warned: (rtuContext.selection && (rtuContext.selection !== null)
        && (rtuContext.selection.count === 1) ? true : false) || extraWarned;

    function recreate()
    {
        area.sourceComponent = undefined;
        area.sourceComponent = areaBinding.value;
    }

    Component
    {
        id: maskingPanel;

        FocusScope
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
                    text: qsTr("Settings in this section can be different for selected servers.");
                }

                Row
                {
                    id: editButtonArea;

                    spacing: Common.SizeManager.spacing.medium;

                    Base.Button
                    {
                        id: editAllButton;

                        KeyNavigation.tab: null;
                        KeyNavigation.backtab: (thisComponent.editBtnPrevTabIsSelf ? editAllButton : null);

                        height: Common.SizeManager.clickableSizes.medium;
                        width: height * 4;
                        text: qsTr("Edit All");

                        onClicked: { thisComponent.warned = true; }

                        Component.onCompleted:
                        {
                            if (thisComponent.editBtnPrevTabIsSelf)
                                forceActiveFocus();
                        }
                    }

                    Base.Text
                    {
                        id: warningText;

                        anchors.verticalCenter: parent.verticalCenter;
                        
                        color: "#666666";
                        wrapMode: Text.Wrap;
                        font.pixelSize: Common.SizeManager.fontSizes.base;
                        text: qsTr("Changes will be applied only if this button is clicked");
                    }
                }
            }
        }
    }

    areaDelegate: Loader
    {
        id: areaDelegateLoader;
    }

    Binding
    {
        id: areaBinding;

        target: area;
        property: "sourceComponent";
        value: (thisComponent.warned ? propertiesDelegate : maskingPanel);
    }

}

