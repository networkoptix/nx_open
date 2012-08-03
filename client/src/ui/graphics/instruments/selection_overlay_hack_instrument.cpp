#include "selection_overlay_hack_instrument.h"
#include <ui/graphics/items/resource/resource_widget.h>

SelectionOverlayHackInstrument::SelectionOverlayHackInstrument(QObject *parent):
    Instrument(Instrument::Scene, makeSet(), parent)
{}

SelectionOverlayHackInstrument::~SelectionOverlayHackInstrument() {
    ensureUninstalled();
}

void SelectionOverlayHackInstrument::installedNotify() {
    connect(scene(), SIGNAL(selectionChanged()), this, SLOT(at_scene_selectionChanged()));
}

void SelectionOverlayHackInstrument::aboutToBeUninstalledNotify() {
    if(scene() != NULL)
        disconnect(scene(), NULL, this, NULL);

    m_singleSelectedWidget.clear();
}

void SelectionOverlayHackInstrument::enabledNotify() {
    showSelectedOverlay(singleSelectedWidget(), false);
}

void SelectionOverlayHackInstrument::aboutToBeDisabledNotify() {
    showSelectedOverlay(singleSelectedWidget(), true);
}

void SelectionOverlayHackInstrument::at_scene_selectionChanged() {
    /* We may get here from scene's destructor. In this case scene() returns NULL. */
    if (scene() == NULL)
        return;

    QList<QGraphicsItem *> selectedItems = scene()->selectedItems();

    QnResourceWidget *newSingleSelectedWidget = selectedItems.size() == 1 ? dynamic_cast<QnResourceWidget *>(selectedItems.first()) : NULL;
    if(newSingleSelectedWidget == singleSelectedWidget())
        return;

    if(isEnabled())
        showSelectedOverlay(singleSelectedWidget(), true);

    m_singleSelectedWidget = newSingleSelectedWidget;

    if(isEnabled())
        showSelectedOverlay(singleSelectedWidget(), false);
}

void SelectionOverlayHackInstrument::showSelectedOverlay(QnResourceWidget *widget, bool show) {
    if(widget == NULL)
        return;

    widget->setDisplayFlag(QnResourceWidget::DisplaySelectionOverlay, show);
}

