// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "director.h"

#include <QtCore/QMetaEnum>
#include <QtQml/QJSValueIterator>
#include <QtWidgets/QApplication>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/from_string.h>
#include <nx/vms/client/desktop/debug_utils/instruments/frame_time_points_provider_instrument.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono_literals;
std::chrono::milliseconds kQuitDelay = 2s;

const QString kItemsProperty = "items";
const QString kResourcesProperty = "resources";

QVariant dumpQObject(const QObject* object, bool withChildren = false)
{
    if (!object)
        return QVariant();

    QVariantMap result;

    // Dump properties.
    for (auto mo = object->metaObject(); mo; mo = mo->superClass())
    {
        for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i)
            result.insert(mo->property(i).name(), mo->property(i).read(object));
    }

    result.insert("objectName", object->objectName());
    result.insert("metaObject.className", object->metaObject()->className());

    if (object->isWidgetType())
    {
        const auto w = qobject_cast<const QWidget*>(object);
        if (w->isVisible())
        {
            QVariantMap geometry;
            geometry.insert("x", w->x());
            geometry.insert("y", w->y());
            geometry.insert("width", w->width());
            geometry.insert("height", w->height());
            result.insert("geometry", geometry);
        }
    }

    if (!withChildren)
        return result;

    const QObjectList children = object->children();
    if (children.empty())
        return result;

    QVariantList resultChildren;

    for (int i = 0; i < children.size(); ++i)
        resultChildren.append(dumpQObject(children.at(i), withChildren));

    result.insert("children", resultChildren);

    return result;
}

} // namespace

Director::Director(WindowContext* windowContext, QObject* parent):
    QObject(parent),
    WindowContextAware(windowContext)
{
    NX_ASSERT_HEAVY_CONDITION(
        []
        {
            for (int i = Qn::FirstItemDataRole; i < Qn::ItemDataRoleCount; ++i)
            {
                if (toString(static_cast<Qn::ItemDataRole>(i)).empty())
                    return false;
            }
            return true;
        }(), "Missing lexical serialization for Qn::ItemDataRole");
}

Director::~Director()
{
}

void Director::quit(bool force)
{
    if (force)
    {
        executeDelayedParented(
            [this] { menu()->trigger(menu::DelayedForcedExitAction); },
            kQuitDelay, this);
    }
    else
    {
        executeDelayedParented(
            [this] { menu()->trigger(menu::ExitAction); },
            kQuitDelay, this);
    }
}

std::vector<qint64> Director::getFrameTimePoints()
{
    return display()->frameTimePointsInstrument()->getFrameTimePoints();
}

void Director::setupJSEngine(QJSEngine* engine)
{
    m_engine = engine;
    m_engine->installExtensions(QJSEngine::ConsoleExtension);

    QJSValue actionEnumValue = m_engine->newQMetaObject(&menu::staticMetaObject);
    m_engine->globalObject().setProperty("action", actionEnumValue);
    m_engine->globalObject().setProperty("director", m_engine->newQObject(this));
}

void Director::subscribe(menu::IDType actionId, QJSValue callback)
{
    connect(action(actionId), &QAction::triggered, this,
        [this, callback]() mutable
        {
            if (!callback.isCallable())
                return;

            const auto actionParameters = menu()->currentParameters(sender());

            QJSValue param = m_engine->newObject();

            // Convert each action parameter to a named property of js object.
            QHashIterator<int, QVariant> i(actionParameters.arguments());
            while (i.hasNext())
            {
                i.next();
                // -1 is a special case for action items
                const QString name = (i.key() == -1)
                    ? kItemsProperty
                    : QString::fromStdString(toString(static_cast<Qn::ItemDataRole>(i.key())));

                // Convert parameter to array or map if possible.
                // We can not use canConvert<T> here because the conversion may fail and original value
                // would not be passed to callback params.
                // Value is copied each time because convert() destroys QVariant value if conversion fails.
                QVariant value = i.value();
                if (value.convert(qMetaTypeId<QVariantList>()))
                {
                    param.setProperty(name, m_engine->toScriptValue(value));
                    continue;
                }

                value = i.value();
                if (value.convert(qMetaTypeId<QVariantMap>()))
                {
                    param.setProperty(name, m_engine->toScriptValue(value));
                    continue;
                }

                param.setProperty(name, m_engine->toScriptValue(i.value()));
            }

            const QnResourceList resources = actionParameters.resources();
            const auto resorcesValue = resources.empty()
                    ? QVariant()
                    : QVariant::fromValue<QnResourceList>(resources);
            param.setProperty(kResourcesProperty, m_engine->toScriptValue<QVariant>(resorcesValue));

            callback.call(QJSValueList{param});
        });
}

void Director::trigger(menu::IDType action, QJSValue parameters)
{
    if (!parameters.isObject())
        return;

    menu::Parameters actionParameters;

    // Gather js object own properties and map them to roles:
    //   "TextRole": value   ->   setArgument(Qn::TextRole, QVariant(value))
    QJSValueIterator it(parameters);
    while (it.hasNext())
    {
        it.next();
        if (it.name() == kResourcesProperty) // special case for action resources
        {
            actionParameters.setResources(it.value().toVariant().value<QnResourceList>());
            continue;
        }
        else if (it.name() == kItemsProperty)
        {
            actionParameters.setItems(it.value().toVariant());
            continue;
        }
        const auto role = nx::reflect::fromString<Qn::ItemDataRole>(
            it.name().toStdString(), Qn::ItemDataRoleCount);
        if (role == Qn::ItemDataRoleCount)
            continue;
        actionParameters.setArgument(role, it.value().toVariant());
    }

    menu()->trigger(action, actionParameters);
}

QJSValue Director::getResources() const
{
    const auto resources = system()->resourcePool()->getResources();

    QVariantList result;
    for (const auto resource: resources)
        result << dumpQObject(resource.data(), /*withChildren*/ true);

    return m_engine->toScriptValue(result);
}

QJSValue Director::execute(const QString& source)
{
    return m_engine->evaluate(source);
}

} // namespace nx::vms::client::desktop
