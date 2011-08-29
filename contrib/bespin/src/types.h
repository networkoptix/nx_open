#ifndef BESPIN_TYPES
#define BESPIN_TYPES

namespace Bespin {

namespace Check {
    enum Type {X = 0, V, O};
}

namespace Navi {
enum Direction {
   N = Qt::UpArrow, S = Qt::DownArrow,
   E = Qt::RightArrow, W = Qt::LeftArrow,
   NW = 5, NE, SE, SW
   };
}

enum BGMode { Plain = 0, Scanlines, BevelV, BevelH };

enum Orientation3D {Sunken = 0, Relief, Raised};

enum AppType
{
    Unknown, GTK, QtDesigner, Plasma, KGet, KDM, KRunner, Dolphin, Opera, BEshell, Arora, KWin,
    KDevelop, Konversation, Amarok, KTorrent, OpenOffice, VLC, KMail, Konqueror, Gwenview
};

namespace Groove {
enum Mode { Line = 0, Groove, Sunken, SunkenGroove };
}
}

#endif // BESPIN_TYPES
