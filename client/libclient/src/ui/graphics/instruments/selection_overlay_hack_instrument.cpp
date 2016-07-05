#include "selection_overlay_hack_instrument.h"

#include <ui/graphics/items/resource/resource_widget.h>

SelectionOverlayHackInstrument::SelectionOverlayHackInstrument(QObject *parent):
    Instrument(Instrument::Scene, makeSet(), parent)
{}

SelectionOverlayHackInstrument::~SelectionOverlayHackInstrument() {
    ensureUninstalled();
}

void SelectionOverlayHackInstrument::installedNotify() {
    connect(scene(), &QGraphicsScene::selectionChanged, this, &SelectionOverlayHackInstrument::at_scene_selectionChanged);
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

bool SelectionOverlayHackInstrument::registeredNotify(QGraphicsItem *item) {
    return base_type::registeredNotify(item);
}

void SelectionOverlayHackInstrument::unregisteredNotify(QGraphicsItem *item) {
    /* Note that scene selection change may be triggered when
     * QnResourceWidget is partially destroyed, but weak pointer is still valid.
     * We don't want that, so we explicitly clear the weak pointer. */ 
    if(item == m_singleSelectedWidget.data())
        m_singleSelectedWidget.clear();

    base_type::unregisteredNotify(item);
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

    widget->setOption(QnResourceWidget::DisplaySelection, show);
}

QnResourceWidget *SelectionOverlayHackInstrument::singleSelectedWidget() {
    return m_singleSelectedWidget.data();
}
