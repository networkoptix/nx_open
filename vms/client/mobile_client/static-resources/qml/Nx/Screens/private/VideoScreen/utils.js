var VideoScreenMode = {
    "Navigation": 0,
    "Ptz": 1
}

function executeDelayed(callback, delay, parent)
{
    if (!callback || delay < 0 || !parent)
    {
        console.error("Wrong parameters for executeDelayed function")
        return
    }

    var TimerCreator =
        function()
        {
            return Qt.createQmlObject("import QtQuick 2.0; Timer {}", parent)
        }

    var timer = new TimerCreator()
    timer.interval = delay;
    timer.repeat = false;
    timer.triggered.connect(
        function()
        {
            callback()
            timer.destroy()
        })
    timer.start()
}

function executeLater(callback, parent)
{
    executeDelayed(callback, 0, parent)
}
