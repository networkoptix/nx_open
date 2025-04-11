// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Dialogs

Dialog
{
    id: dialog

    width: 1000
    height: 400
    title: "Name-Value Table Test"

    modality: Qt.ApplicationModal

    contentItem: RowLayout
    {
        spacing: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8

        ColumnLayout
        {
            id: inputLayout
            width: 400

            spacing: 4

            Label
            {
                text: "Input:"
            }

            ScrollView
            {
                width: 400
                Layout.fillHeight: true

                TextArea
                {
                    id: inputText

                    text: `[\n` +
                        `  {"name": "Brand", "values": ["Lada with long name", "Lada", "Mercedes"]},\n` +
                        `  {"name": "Color", "colors": ["#FF0000", "#00FF00", "#0000FF"], "values": ["Red", "Green", "Blue"]},\n` +
                        `  {"name": "License Plate Country", "values": ["Russia", "Kazakhstan"]},\n` +
                        `  {"name": "License Plate Number", "values": ["A001AA", "KEN560"]},\n` +
                        `  {"name": "License Plate State/Province", "values": ["77", "05"]},\n` +
                        `  {"name": "Model", "values": ["2106", "EQE"]},\n` +
                        `  {"name": "License Plate", "values": ["Present"]},\n` +
                        `  {"name": "Best Shot Type", "values": ["URL"]}\n` +
                        `]`

                    onTextChanged:
                    {
                        nameValueTable.items = JSON.parse(text)
                    }
                }
            }

            GridLayout
            {
                Layout.fillWidth: true
                columns: 4
                rowSpacing: 4
                columnSpacing: 8

                Label
                {
                    text: "Max row count:"
                }

                SpinBox
                {
                    value: nameValueTable.maxRowCount

                    onValueChanged:
                    {
                        nameValueTable.maxRowCount = value
                    }
                }

                Label
                {
                    text: "Max line count:"
                }

                SpinBox
                {
                    value: nameValueTable.maximumLineCount

                    onValueChanged:
                    {
                        nameValueTable.maximumLineCount = value
                    }
                }

                Label
                {
                    text: "Label Fraction:"
                }

                DoubleSpinBox
                {
                    dFrom: 0
                    dTo: 1
                    editable: true
                    dValue: nameValueTable.labelFraction
                    dStepSize: 0.1
                    decimals: 1

                    onValueModified:
                    {
                        nameValueTable.labelFraction = dValue
                        nameValueTable.update()
                    }
                }

            }
        }

        ColumnLayout
        {
            id: outputLayout

            Layout.fillWidth: true

            spacing: 4

            Label
            {
                text: "Output:"
            }

            Rectangle
            {
                color: ColorTheme.colors.dark5
                Layout.fillWidth: true
                Layout.fillHeight: true

                NameValueTable
                {
                    id: nameValueTable

                    anchors.fill: parent
                    width: 400
                }
            }

            GridLayout
            {
                Layout.fillWidth: true
                columns: 4
                rowSpacing: 4
                columnSpacing: 8

                Label
                {
                    text: "width"
                }

                Label
                {
                    text: nameValueTable.width
                    color: "white"
                }

                Label
                {
                    text: "height"
                }

                Label
                {
                    text: nameValueTable.height
                    color: "white"
                }
            }
        }
    }

    buttonBox: DialogButtonBox
    {
        id: buttons

        standardButtons: DialogButtonBox.Close
    }
}
