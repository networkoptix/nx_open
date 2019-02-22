import QtQuick 2.6
import Nx.Controls 1.0
import Nx 1.0

IconButton
{
    property bool forward: true

    backgroundColor: ColorTheme.transparent(ColorTheme.base1, 0.2)
    disabledBackgroundColor: ColorTheme.transparent(ColorTheme.base1, 0.1)

    icon.source: forward ? lp("/images/playback_fwd.svg") : lp("/images/playback_rwd.svg")
    icon.width: 24
    icon.height: icon.height
    padding: 0
    width: 40
    height: width
}
