import QtQuick 2.4;
import QtQuick.Controls 1.1;

import "../common" as Common
import "../settings" as Settings;
import "../controls/base" as Base;
import "../dialogs" as Dialogs;
import "../controls/rtu" as Rtu;

import networkoptix.rtu 1.0 as NxRtu;

FocusScope
{
    id: thisComponent;
    
    property var selectedServersModel;
    property bool parametersChanged: systemAndPasswordSettings.changed 
        || ipPortSettings.changed || dateTimeSettings.changed;

    anchors.fill: parent;
    activeFocusOnTab: false;

    Dialogs.MessageDialog
    {
        id: warningDialog;

        title: "Warning";
        buttons: (NxRtu.Buttons.Cancel | NxRtu.Buttons.Ok);
        styledButtons: NxRtu.Buttons.Ok;
        cancelButton: NxRtu.Buttons.Cancel;
        
        dontShowText: "Do not show warnings";

        function showWarnings(warnings, onFinishedCallback, onCanceledCallback)
        {
            impl.onFinishedCallback = onFinishedCallback;
            impl.onCanceledCallback = onCanceledCallback;
            impl.warnings = warnings;
            impl.show(0);
        }

        onButtonClicked:
        {
            if (id === NxRtu.Buttons.Ok)
            {
                impl.show(impl.currentIndex + 1);
            }
            else if (impl.onCanceledCallback)
            {
                impl.onCanceledCallback();
            }
        }

        onDontShowNextTimeChanged: { rtuContext.showWarnings = !dontShow; }

        property QtObject impl: QtObject
        {
            id: impl;

            property var onFinishedCallback;
            property var onCanceledCallback;
            property var warnings;
            property int currentIndex: 0;

            function show(index)
            {
                if (!rtuContext.showWarnings
                    || ((index < 0) || (index >= impl.warnings.length)))
                {
                    if (impl.onFinishedCallback)
                        impl.onFinishedCallback();
                    return;
                }

                impl.currentIndex = index;
                warningDialog.message = impl.warnings[index];
                warningDialog.show();
            }
        }
    }

    Dialogs.ErrorDialog
    {
        id: errorDialog;
    }

    function applyChanges()
    {
        if (rtuContext.selection.safeMode)
        {
            rtuContext.changesManager().clearChanges();
            errorDialog.message = safeModeWarningText.text;
            errorDialog.show();
            return;
        }

        var children = settingsColumn.children;
        var childrenCount = children.length;
        var changesCount = 0;
        var entitiesCount = 0;

        var warnings = [];
        for (var i = 0; i !== childrenCount; ++i)
        {
            var child = children[i];
            if (!child.hasOwnProperty("tryApplyChanges"))
                continue;
            ++entitiesCount;
            
            if (!child.tryApplyChanges(warnings))
                break;
            ++changesCount;
        }

        if (changesCount && (changesCount == entitiesCount))
        {
            if (warnings.length)
            {
                var finishCallback = function()
                {
                    rtuContext.changesManager().applyChanges();
                };

                var cancelCallback = function()
                {
                    rtuContext.changesManager().clearChanges();
                }

                warningDialog.showWarnings(warnings
                    , finishCallback, cancelCallback);
            }
            else
                rtuContext.changesManager().applyChanges();
        }
        else
        {
            rtuContext.changesManager().clearChanges();
        }
    }
    
    Connections
    {
        target: rtuContext.selection;

        function processChanges(item)
        {
            var updateFunc = function() { item.recreate(); }

            if (item.warned && !item.changed)
                updateFunc();
            else
                outdatedWarning.addOnUpdateAction(updateFunc);
        }

        onInterfaceSettingsChanged: { processChanges(ipPortSettings); }
        onDateTimeSettingsChanged: { processChanges(dateTimeSettings); }
        onSystemSettingsChanged: { processChanges(systemAndPasswordSettings); }
    }

    Rtu.OutdatedWarningPanel
    {
        id: outdatedWarning;

        width: parent.width;
    }

    Rectangle
    {
        id: safeModeWarning;

        height: safeModeWarningText.height + Common.SizeManager.spacing.medium * 2;

        anchors
        {
            left: parent.left;
            right: parent.right;
        }
        visible: rtuContext.selection.safeMode;

        color: "#F0E9D9"
        border.color: "#DBC9B7";

        Base.Text
        {
            id: safeModeWarningText;

            anchors
            {
                left: parent.left;
                right: parent.right;
                leftMargin: Common.SizeManager.spacing.large;
                rightMargin: Common.SizeManager.spacing.large;

                verticalCenter: parent.verticalCenter;
            }

            thin: true;
            font.pixelSize: Common.SizeManager.fontSizes.medium;

            text: (rtuContext.selection.count == 1
                ? qsTr("Selected server is in Safe Mode.\nNo changes can be applied")
                : qsTr("Some of selected servers are in Safe Mode.\nNo changes can be applied"))
            color: "#4B1010";
        }
    }

    ScrollView
    {
        width: parent.width;
        anchors
        {
            top: (outdatedWarning.visible || !safeModeWarning.visible
                  ? outdatedWarning.bottom : safeModeWarning.bottom);
            bottom: buttonsPanel.top;
        }
        
        clip: true;
        activeFocusOnTab: false;

        Flickable
        {
            flickableDirection: Flickable.VerticalFlick;
            contentHeight: settingsColumn.height;

            anchors
            {
                fill: parent;

                topMargin: Common.SizeManager.fontSizes.base;
                bottomMargin: Common.SizeManager.spacing.base;
                leftMargin: Common.SizeManager.spacing.medium;
                rightMargin: Common.SizeManager.spacing.medium;
            }

            Column
            {
                id: settingsColumn;
                width: parent.width;

                spacing: Common.SizeManager.spacing.large;

                Settings.IpPortSettings
                {
                    id: ipPortSettings;

                    extraWarned: rtuContext.selection.safeMode;
                    enabled: !rtuContext.selection.safeMode;
                }

                Settings.DateTimeSettings
                {
                    id: dateTimeSettings;

                    pseudoEnabled: !rtuContext.selection.safeMode;
                }

                Settings.SystemAndPasswordSetting
                {
                    id: systemAndPasswordSettings;

                    extraWarned: rtuContext.selection.safeMode;
                    enabled: !rtuContext.selection.safeMode;
                }

                Settings.ActionSettings
                {
                    id: actionSettings;

                    nextTab: applyButton;
                    flags: rtuContext.selection.SystemCommands;
                }

                Base.EmptyCell {}
            }
        }
    }
   
    Item
    {
        id: buttonsPanel;

        height: Common.SizeManager.clickableSizes.medium * 2;
        anchors
        {
            left: parent.left;
            right: parent.right;
            bottom: parent.bottom;
        }

        Base.ThickDelimiter 
        {
            id: delimiter;
            
            anchors
            {
                left: parent.left;
                right: parent.right;
                top: parent.top;
            }
        }

        Row
        {
            id: buttonsRow;

            spacing: Common.SizeManager.spacing.base;
            
            anchors
            {
                left: parent.left;
                right: parent.right;
                top: delimiter.bottom;
                bottom: parent.bottom;
                leftMargin: Common.SizeManager.spacing.medium;
                rightMargin: Common.SizeManager.spacing.medium;
            }

            layoutDirection: Qt.RightToLeft;
    
            Base.StyledButton
            {
                id: applyButton;
                text: qsTr("Apply changes");
    
                height: Common.SizeManager.clickableSizes.medium;
                width: height * 4;
                
                anchors.verticalCenter: parent.verticalCenter;
                enabled: systemAndPasswordSettings.changed || ipPortSettings.changed || dateTimeSettings.changed;
                
                onClicked: thisComponent.applyChanges();
            }
    
            Base.Button
            {
                text: "Cancel";

                anchors.verticalCenter: parent.verticalCenter;
                height: Common.SizeManager.clickableSizes.medium;
                width: height * 3;
                
                Keys.onTabPressed: {}

                enabled: applyButton.enabled;
                onClicked:
                {
                    rtuContext.selectionChanged();
                    rtuContext.changesManager().clearChanges();
                }
            }
        }
    }
}

