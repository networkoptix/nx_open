import QtQuick 2.0
import Nx 1.0
import Nx.Test 1.0

Item
{
    property bool running: false
    property int startDelay: 5000

    QtObject
    {
        id: d

        property string state: "idle"

        property int moveCount: -1
        readonly property int maxMove: 4
        readonly property int minInterval: 500
        readonly property int maxInterval: 30000

        function log(message)
        {
            console.log("LiteClientTest: ", message)
        }

        function error(message)
        {
            console.log("LiteClientTest: Error: ", message)
            stop()
        }

        function findPage(pageName)
        {
            for (var i = 0; i < stackView.depth; ++i)
            {
                var item = stackView.get(i)
                if (item && item.objectName == pageName)
                    return item
            }
            return null
        }

        function getGrid()
        {
            var resScreen = d.findPage("resourcesScreen")
            if (!resScreen)
                error("Resources screen not found.")

            var grid = helper.findChildObject(resScreen, "camerasGrid")
            if (!grid)
                error("No cameras grid found")

            return grid
        }


        function start()
        {
            log("Starting")
            state = "move"
            timer.interval = startDelay
            timer.start()
        }

        function stop()
        {
            log("Stopping")
            state = "idle"
        }

        function moveCursor()
        {
            var grid = getGrid()
            if (!grid)
                return

            if (grid.count == 0)
                return

            if (moveCount < 0)
                moveCount = helper.random(0, maxMove - 1)

            --moveCount

            var index = helper.random(0, grid.count)
            log("Moving to item " + index)
            grid.currentIndex = index

            if (moveCount < 0)
                state = "camera_in"

            timer.interval = helper.random(minInterval, maxInterval)
            timer.start()
        }

        function openCamera()
        {
            var grid = getGrid()
            if (!grid)
                return

            grid.currentItem.clicked()

            state = "camera_out"
            log("Opening camera")
            timer.interval = helper.random(minInterval, maxInterval)
            timer.start()
        }

        function closeCamera()
        {
            Workflow.openResourcesScreen()
            log("Closing camera")
            start()
        }
    }

    QmlTestHelper { id: helper }

    Timer
    {
        id: timer

        onTriggered:
        {
            if (d.state == "idle")
                return
            else if (d.state == "move")
                d.moveCursor()
            else if (d.state == "camera_in")
                d.openCamera()
            else if (d.state == "camera_out")
                d.closeCamera()
        }
    }

    onRunningChanged:
    {
        if (running)
            d.start()
        else
            d.stop()
    }
}
