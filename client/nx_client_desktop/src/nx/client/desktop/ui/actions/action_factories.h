#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/client/desktop/ui/actions/action_fwd.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class Factory: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    Factory(QObject* parent);
    virtual QList<QAction*> newActions(const Parameters& parameters, QObject* parent = nullptr);
    virtual QMenu* newMenu(const Parameters& /*parameters*/, QWidget* /*parentWidget*/);
};


class OpenCurrentUserLayoutFactory: public Factory
{
    Q_OBJECT
public:
    OpenCurrentUserLayoutFactory(QObject* parent);
    virtual QList<QAction*> newActions(const Parameters& parameters, QObject* parent) override;
};


class PtzPresetsToursFactory: public Factory
{
    Q_OBJECT
public:
    PtzPresetsToursFactory(QObject* parent);
    virtual QList<QAction*> newActions(const Parameters& parameters, QObject* parent) override;
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
    virtual QList<QAction*> newActions(const Parameters& parameters, QObject* parent) override;
};

class LayoutTourSettingsFactory: public Factory
{
    Q_OBJECT
public:
    LayoutTourSettingsFactory(QObject* parent);
    virtual QList<QAction*> newActions(const Parameters& parameters, QObject* parent) override;
};

class AnalyticsModeActionFactory: public Factory
{
    Q_OBJECT
public:
    AnalyticsModeActionFactory(QObject *parent = NULL): Factory(parent) {}
    virtual QList<QAction *> newActions(const Parameters &parameters, QObject *parent) override;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
