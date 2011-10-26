#include "sceneinputstopinstrument.h"

namespace {
    QEvent::Type sceneEventTypes[] = {
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick
    };

} // anonymous namespace

SceneInputStopInstrument::SceneInputStopInstrument(QObject *parent):
    Instrument(makeSet(sceneEventTypes), makeSet(), makeSet(), false, parent)
{}

