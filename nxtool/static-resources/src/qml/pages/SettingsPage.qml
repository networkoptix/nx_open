import QtQuick 2.1;
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
   
    Dialogs.MessageDialog
    {
        id: warningDialog;

        title: "Warning";
        buttons: (NxRtu.Buttons.Cancel | NxRtu.Buttons.Ok);
        styledButtons: NxRtu.Buttons.Ok;
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

    function applyChanges()
    {
        console.log("n\n\n\n\n\n\n")
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
            {
                rtuContext.changesManager().applyChanges();
            }
        }
        else
        {
            rtuContext.changesManager().clearChanges();
        }

        //rtuContext.changesManager().minimizeProgress();
        //rtuContext.changesManager().changesProgressModel().removeChangeProgress(0);
    }
    
    Rtu.OutdatedWarningPanel
    {
        id: outdatedWarning;

        show: rtuContext.selectionModel().selectionOutdated;
        width: parent.width;

        onUpdateClicked: { rtuContext.selectionModel().selectionChanged(); }
    }

    ScrollView
    {
        width: parent.width;
        anchors
        {
            top: outdatedWarning.bottom;
            bottom: buttonsPanel.top;
        }
        
        clip: true;

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
                }

                Settings.DateTimeSettings
                {
                    id: dateTimeSettings;
                }

                Settings.SystemAndPasswordSetting
                {
                    id: systemAndPasswordSettings;
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

