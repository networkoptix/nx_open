import QtQuick 2.0
import Nx 1.0

Object
{
    id: measurer

    property real value
    readonly property real speed: d.speed
    property bool active: false

    QtObject
    {
        id: d

        property int kNoticeableMeasures: 16

        property real speed: 0
        property var measures: []

        function addMeasure(value, time)
        {
            while (measures.length >= kNoticeableMeasures)
                measures.shift()

            measures.push({ "value": value, "time": time })
        }

        function updateSpeed()
        {
            var l = measures.length
            if (l <= 1)
                return

            var firstTime = measures[0].time
            var firstValue = measures[0].value
            var lastTime = measures[l - 1].time
            var lastValue = measures[l - 1].value

            speed = (lastValue - firstValue) / Math.max(1, lastTime - firstTime) * 1000
        }
    }

    onValueChanged:
    {
        if (!active)
            return

        d.addMeasure(measurer.value, (new Date()).getTime())
        d.updateSpeed()
    }

    onActiveChanged:
    {
        if (!active)
        {
            d.addMeasure(measurer.value, (new Date()).getTime())
            d.updateSpeed()
            return
        }

        d.speed = 0
        d.measures = []

        d.addMeasure(value, (new Date()).getTime())
    }
}
