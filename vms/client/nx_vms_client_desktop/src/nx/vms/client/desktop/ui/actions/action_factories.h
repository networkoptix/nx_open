// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QWidget;

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

class Factory: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    using ActionList = QList<QAction*>;
    Factory(QObject* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent = nullptr);
    virtual QMenu* newMenu(const Parameters& /*parameters*/, QWidget* /*parentWidget*/);
};


class OpenCurrentUserLayoutFactory: public Factory
{
    Q_OBJECT
public:
    OpenCurrentUserLayoutFactory(QObject* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};


class PtzPresetsToursFactory: public Factory
{
    Q_OBJECT
public:
    PtzPresetsToursFactory(QObject* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class EdgeNodeFactory: public Factory
{
    Q_OBJECT
public:
    EdgeNodeFactory(QObject* parent);
    virtual QMenu* newMenu(const Parameters& parameters, QWidget *parentWidget) override;
};

class AspectRatioFactory: public Factory
{
    Q_OBJECT
public:
    AspectRatioFactory(QObject* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class ShowreelSettingsFactory: public Factory
{
    Q_OBJECT
public:
    ShowreelSettingsFactory(QObject* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class WebPageFactory: public Factory
{
    Q_OBJECT
public:
    WebPageFactory(QObject* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

class ShowOnItemsFactory: public Factory
{
    Q_OBJECT
public:
    ShowOnItemsFactory(QObject* parent);
    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;

private:
    QAction* initInfoAction(const Parameters& parameters, QObject* parent);
    QAction* initObjectsAction(const Parameters& parameters, QObject* parent);
    QAction* initRoiAction(const Parameters& parameters, QObject* parent);
    QAction* initHotspotsAction(const Parameters& parameters, QObject* parent);
    QAction* initToolbarAction(const Parameters& parameters, QObject* parent);
};

class SoundPlaybackActionFactory: public Factory
{
    Q_OBJECT
public:
    SoundPlaybackActionFactory(QObject* parent);

    virtual ActionList newActions(const Parameters& parameters, QObject* parent) override;
};

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
