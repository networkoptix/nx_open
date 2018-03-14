import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Rectangle
{
    id: control

    property int horizontalPadding: 12
    property int verticalPadding: 12
    property alias progressDuration: progressAnmiation.duration

    width: bodyRow.width + horizontalPadding
    height: bodyRow.height + verticalPadding * 2
    color: ColorTheme.transparent(ColorTheme.base8, 0.95)
    radius: 2

    visible: false

    onWidthChanged:
    {
        if (!progressAnmiation.running)
            return;

        progressAnmiation.stop()
        progressAnmiation.from = progressRect.width
        progressAnmiation.to = control.width
        progressAnmiation.start()
    }

    Rectangle
    {
        id: progressRect
        color: Qt.rgba(0, 1, 0, 0.3)
        height: parent.height
        width: 0
    }


    PropertyAnimation
    {
        id: progressAnmiation

        target: progressRect
        properties: "width"
        from: 0
        to: control.width
        duration: 1000
    }

    function showHint(text, iconPath)
    {
        hideTimer.stop()

        hintText.visible = true

        border.color = control.color

        loader.sourceComponent = textComponent
        loader.item.text = text
        loader.item.color = ColorTheme.brightText

        visualDataLoader.sourceComponent = imageComponent
        visualDataLoader.item.source = iconPath

        control.visible = true
        progressAnmiation.stop()
        progressAnmiation.from = 0
        progressAnmiation.to = control.width
        progressAnmiation.start()
    }

    function showCustomProcess(component, iconPath)
    {
        hideTimer.stop()

        hintText.visible = false
        stopProgressAnimation()

        border.color = control.color

        loader.sourceComponent = component

        visualDataLoader.sourceComponent = imageComponent
        visualDataLoader.item.source = iconPath

        control.visible = true
    }

    function showPreloader(text)
    {
        hideTimer.stop();

        hintText.visible = false
        stopProgressAnimation()

        border.color = control.color

        loader.sourceComponent = textComponent
        loader.item.text = text
        loader.item.color = ColorTheme.brightText

        visualDataLoader.sourceComponent = dotsPreloader
        control.visible = true
    }

    function showSuccess(text, showBorder)
    {
        showResult(text, showBorder, true)
    }

    function showFailure(text, showBorder)
    {
        showResult(text, showBorder, false)
    }

    function showResult(text, showBorder, success)
    {
        hideTimer.restart()

        hintText.visible = false
        stopProgressAnimation()

        var customColor = success
            ? ColorTheme.brightText
            : ColorTheme.red_l2

        if (showBorder)
            border.color = customColor
        else
            border.color = control.color

        loader.sourceComponent = textComponent
        loader.item.text = text
        loader.item.color = customColor

        visualDataLoader.sourceComponent = imageComponent
        visualDataLoader.item.source = success
            ? "qrc:///images/soft_trigger/confirmation_success.png"
            : "qrc:///images/soft_trigger/confirmation_failure.png"

        control.visible = true
    }

    function stopProgressAnimation()
    {
        progressAnmiation.stop()
        progressRect.width = 0
    }

    function hideDelayed()
    {
        hideTimer.restart();
    }

    function hide()
    {
        loader.sourceComponent = dummyComponent
        visualDataLoader.sourceComponent = dummyComponent

        hideTimer.stop()
        control.visible = false
    }

    Row
    {
        id: bodyRow

        x: control.horizontalPadding
        y: control.verticalPadding
        height: 24

        Text
        {
            id: hintText

            height: 24
            wrapMode: Text.NoWrap
            verticalAlignment: Text.AlignVCenter
            color: "lightblue"
            font.pixelSize: 14
            font.weight: Font.Normal
            text: qsTr("Hold to:")
            rightPadding: 8
        }

        Loader
        {
            id: loader
            anchors.verticalCenter: parent.verticalCenter
        }

        Item
        {
            width: 48
            height: 48
            anchors.verticalCenter: parent.verticalCenter

            Loader
            {
                id: visualDataLoader
                anchors.centerIn: parent
            }
        }
    }

    Timer
    {
        id: hideTimer

        interval: 3000
        onTriggered: control.visible = false
    }

    Component
    {
        id: dummyComponent

        Item {}
    }

    Component
    {
        id: textComponent

        Text
        {
            height: 24
            wrapMode: Text.NoWrap
            verticalAlignment: Text.AlignVCenter
            color: ColorTheme.brightText
            font.pixelSize: 14
            font.weight: Font.Normal
        }
    }

    Component
    {
        id: imageComponent

        Image {}
    }

    Component
    {
        id: dotsPreloader

        ThreeDotBusyIndicator
        {
            color: ColorTheme.brightText
            dotSpacing: 3
            dotRadius: 4
        }
    }
}
