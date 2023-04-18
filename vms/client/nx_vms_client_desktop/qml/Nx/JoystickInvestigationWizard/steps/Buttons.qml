// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQml.Models
import Qt.labs.qmlmodels

import Nx
import Nx.Controls
import Nx.JoystickInvestigationWizard

import './buttons_step_utils.mjs' as ButtonsStepUtils

WizardStep
{
    id: buttonsStep

    property var buttonRows: []
    property var previousState: null

    StackView.onDeactivated:
    {
        joystickInvestigationDialog.collectedData.buttons =
            ButtonsStepUtils.getButtonsCollectedData.bind(buttonsStep)()
    }

    Column
    {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 5

        Label
        {
            width: parent.width

            wrapMode: Text.WordWrap
            text: qsTr("Press each button several times and write the names of these buttons "
                + "in the table below.")
        }

        TableView
        {
            width: parent.width
            height: 250
            topMargin: 20
            clip: true

            model: TableModel
            {
                TableModelColumn { display: "bitNumber" }
                TableModelColumn { display: "name" }
                TableModelColumn { display: "value" }

                rows: buttonRows
            }

            delegate: DelegateChooser
            {
                DelegateChoice
                {
                    column: 0
                    ItemDelegate
                    {
                        implicitWidth: 50
                        implicitHeight: 30

                        Label
                        {
                            text: model.display
                            anchors.centerIn: parent
                        }
                    }
                }

                DelegateChoice
                {
                    column: 1
                    ItemDelegate
                    {
                        implicitWidth: 250
                        implicitHeight: 30

                        TextField
                        {
                            id: textField

                            text: model.display
                            anchors.centerIn: parent

                            onEditingFinished: buttonRows[model.row].name = textField.text
                        }
                    }
                }

                DelegateChoice
                {
                    column: 2
                    ItemDelegate
                    {
                        implicitWidth: 50
                        implicitHeight: 30

                        Label
                        {
                            text: model.display
                            anchors.centerIn: parent
                        }
                    }
                }
            }
        }
    }

    Connections
    {
        id: connections

        enabled: buttonsStep.StackView.status === StackView.Active
        target: joystickInvestigationDialog.currentDevice

        function onStateChanged(newState)
        {
            ButtonsStepUtils.pollButtons.bind(buttonsStep)(newState)
        }
    }
}
