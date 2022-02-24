// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "selection_overlay_tune_instrument.h"

#include <ui/graphics/items/resource/resource_widget.h>

SelectionOverlayTuneInstrument::SelectionOverlayTuneInstrument(QObject *parent):
    Instrument(Instrument::Scene, makeSet(), parent)
{}

SelectionOverlayTuneInstrument::~SelectionOverlayTuneInstrument() {
    ensureUninstalled();
}

void SelectionOverlayTuneInstrument::installedNotify() {
    connect(scene(), &QGraphicsScene::selectionChanged, this, &SelectionOverlayTuneInstrument::at_scene_selectionChanged);
}

void SelectionOverlayTuneInstrument::aboutToBeUninstalledNotify() {
    if (scene())
        scene()->disconnect(this);

    m_singleSelectedWidget.clear();
}

void SelectionOverlayTuneInstrument::enabledNotify() {
    showSelectedOverlay(singleSelectedWidget(), false);
}

void SelectionOverlayTuneInstrument::aboutToBeDisabledNotify() {
    showSelectedOverlay(singleSelectedWidget(), true);
}

bool SelectionOverlayTuneInstrument::registeredNotify(QGraphicsItem *item) {
    return base_type::registeredNotify(item);
}

void SelectionOverlayTuneInstrument::unregisteredNotify(QGraphicsItem *item) {
    /* Note that scene selection change may be triggered when
     * QnResourceWidget is partially destroyed, but weak pointer is still valid.
     * We don't want that, so we explicitly clear the weak pointer. */
    if(item == m_singleSelectedWidget.data())
        m_singleSelectedWidget.clear();

    base_type::unregisteredNotify(item);
}

void SelectionOverlayTuneInstrument::at_scene_selectionChanged() {
    /* We may get here from scene's destructor. In this case scene() returns nullptr. */
    if (scene() == nullptr)
        return;

    QList<QGraphicsItem *> selectedItems = scene()->selectedItems();

    QnResourceWidget *newSingleSelectedWidget = selectedItems.size() == 1 ? dynamic_cast<QnResourceWidget *>(selectedItems.first()) : nullptr;
    if(newSingleSelectedWidget == singleSelectedWidget())
        return;

    if(isEnabled())
        showSelectedOverlay(singleSelectedWidget(), true);

    m_singleSelectedWidget = newSingleSelectedWidget;

    if(isEnabled())
        showSelectedOverlay(singleSelectedWidget(), false);
}

void SelectionOverlayTuneInstrument::showSelectedOverlay(QnResourceWidget *widget, bool show) {
    if(widget == nullptr)
        return;

    widget->setOption(QnResourceWidget::DisplaySelection, show);
}

QnResourceWidget *SelectionOverlayTuneInstrument::singleSelectedWidget() {
    return m_singleSelectedWidget.data();
}
