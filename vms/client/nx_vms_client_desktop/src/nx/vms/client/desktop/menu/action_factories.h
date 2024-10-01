// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/window_context_aware.h>

#include "action_fwd.h"

class QWidget;

namespace nx::vms::client::desktop {
namespace menu {

class Manager;

class Factory: public QObject, public WindowContextAware
{
    Q_OBJECT
public:
    using ActionList = QList<QAction*>;
    Factory(Manager* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent = nullptr);
    virtual QMenu* newMenu(const Parameters& /*parameters*/, QWidget* /*parentWidget*/);
};


class OpenCurrentUserLayoutFactory: public Factory
{
    Q_OBJECT
public:
    OpenCurrentUserLayoutFactory(Manager* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};


class PtzPresetsToursFactory: public Factory
{
    Q_OBJECT
public:
    PtzPresetsToursFactory(Manager* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class EdgeNodeFactory: public Factory
{
    Q_OBJECT
public:
    EdgeNodeFactory(Manager* parent);
    virtual QMenu* newMenu(const Parameters& parameters, QWidget *parentWidget) override;
};

class AspectRatioFactory: public Factory
{
    Q_OBJECT
public:
    AspectRatioFactory(Manager* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class ShowreelSettingsFactory: public Factory
{
    Q_OBJECT
public:
    ShowreelSettingsFactory(Manager* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class WebPageFactory: public Factory
{
    Q_OBJECT
public:
    WebPageFactory(Manager* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class ShowOnItemsFactory: public Factory
{
    Q_OBJECT
public:
    ShowOnItemsFactory(Manager* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;

private:
    QAction* initInfoAction(const Parameters& parameters, QObject* parent);
    QAction* initObjectsAction(const Parameters& parameters, QObject* parent);
    QAction* initRoiAction(const Parameters& parameters, QObject* parent);
    QAction* initHotspotsAction(const Parameters& parameters, QObject* parent);
    QAction* initToolbarAction(const Parameters& parameters, QObject* parent);
};

} // namespace menu
} // namespace nx::vms::client::desktop
