import QtQuick 2.6

MouseArea
{
    id: wheelSwitchArea

    property int maxConsequentRequests: 0

    signal nextRequested()
    signal previousRequested()

    property int __currentRequests: 0

    onMaxConsequentRequestsChanged: __currentRequests = 0

    onWheel:
    {
        if (wheel.angleDelta.y > 0)
        {
            if (__currentRequests > 0)
                __currentRequests = 0

            if (maxConsequentRequests <= 0)
            {
                previousRequested()
                return
            }

            if (__currentRequests > -maxConsequentRequests)
            {
                --__currentRequests
                previousRequested()
                timer.start()
            }
        }
        else
        {
            if (__currentRequests < 0)
                __currentRequests = 0

            if (maxConsequentRequests <= 0)
            {
                nextRequested()
                return
            }

            if (__currentRequests < maxConsequentRequests)
            {
                timer.start()
                ++__currentRequests
                nextRequested()
            }
        }
    }

    Timer
    {
        id: timer
        interval: 300
        onTriggered: __currentRequests = 0
    }
}
