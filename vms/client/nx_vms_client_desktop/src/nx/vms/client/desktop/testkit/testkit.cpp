// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "testkit.h"

#include <QtCore/QBuffer>
#include <QtCore/QJsonObject>
#include <QtGui/QCloseEvent> //< TODO: Move event posting to input utils.
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQml/QQmlEngine>
#include <QtWidgets/QApplication>

#include <nx/build_info.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>

#include "highlighter.h"
#include "model_index_wrapper.h"
#include "object_wrapper.h"
#include "utils.h"

class QnWorkbenchContext;

namespace nx::vms::client::desktop::testkit {

namespace {

template <typename R>
QJsonValue rectToJson(const R& r)
{
    return QJsonObject{
        {"x", r.x()},
        {"y", r.y()},
        {"width", r.width()},
        {"height", r.height()}
    };
}

} // namespace

TestKit::TestKit(QObject* parent): base_type(parent), m_highlighter(std::make_unique<Highlighter>())
{
    ModelIndexWrapper::registerMetaType();

    qApp->installEventFilter(this);
}

TestKit::~TestKit()
{
    ensureAsyncActionFinished();
    qApp->removeEventFilter(this);
}

void TestKit::setup(QJSEngine* engine)
{
    engine->installExtensions(QJSEngine::AllExtensions);

    // Create a new instance and pass the ownership to the engine.
    auto testkit = new TestKit();
    engine->globalObject().setProperty(kName, engine->newQObject(testkit));
}

void TestKit::onEvent(QJSValue observers)
{
    m_eventObservers = observers;
}

namespace {

QString eventName(const QEvent* event)
{
    static const QMap<QEvent::Type, QString> eventNames{
        {QEvent::Hide, "QHideEvent"},
        {QEvent::Show, "QShowEvent"},
        {QEvent::MouseButtonDblClick, "QMouseEvent"},
        {QEvent::MouseButtonPress, "QMouseEvent"},
        {QEvent::MouseButtonRelease, "QMouseEvent"},
        {QEvent::MouseMove, "QMouseEvent"},
    };

    return eventNames.value(event->type());
}

QPoint valueToPoint(const QJSValue& value)
{
    QPoint result;
    if (value.hasOwnProperty("x"))
        result.setX(value.property("x").toInt());
    if (value.hasOwnProperty("y"))
        result.setY(value.property("y").toInt());
    return result;
}

} // namespace

bool TestKit::eventFilter(QObject* obj, QEvent* event)
{
    if (m_eventObservers.isArray())
    {
        const auto name = eventName(event);
        if (!name.isEmpty())
        {
            // Iterate through all observers.
            const auto length = m_eventObservers.property("length").toInt();
            for (int i = 0; i < length; ++i)
            {
                // Check event name.
                QJSValue observer = m_eventObservers.property(i);
                if (observer.property("event").toString() != name)
                    continue;

                // Check object parameters.
                if (!utils::objectMatches(obj, observer.property("match")))
                    continue;

                // Invoke callback.
                QJSValue callback = observer.property("callback");
                if (!callback.isCallable())
                    continue;
                QJSValueList args;
                args << wrapQObject(obj);
                callback.call(args);
            }
        }
    }

    return base_type::eventFilter(obj, event);
}

QJSValue TestKit::wrapQObject(QObject* obj)
{
    // Avoid crash when `this` is not currently exposed to the scripting engine.
    auto engine = qjsEngine(this);
    if (!engine)
        return QJSValue();

    const auto ownership = QQmlEngine::objectOwnership(obj);
    QQmlEngine::setObjectOwnership(obj, ownership);
    return engine->newQObject(obj);
}

QJSValue TestKit::wrapQVariant(QVariant value)
{
    if (auto object = qvariant_cast<QObject*>(value))
        return wrapQObject(object);
    return qjsEngine(this)->toScriptValue(value);
}

QJsonObject TestKit::execute(const QString& source)
{
    // Avoid crash, just in case...
    auto engine = qjsEngine(this);
    if (!engine)
    {
        return QJsonObject{
            {"error", 1},
            {"errorString", "Missing QJSEngine"}
        };
    }

    // Execute the provided script.
    // Event loop may execute here as a result of script actions!
    const QJSValue result = engine->evaluate(source);

    // Return error description if the script fails.
    if (result.isError())
    {
        return QJsonObject{
            {"error", 1},
            {"errorString", result.toString()}
        };
    }

    // Construct JSON representation of the result.
    QJsonObject response;
    const auto resultVariant = result.toVariant();
    auto jsonResult = resultVariant.toJsonValue();

    // Complex values like QObject don't have JSON representation,
    // so we need to provide additional type information.
    // For convinience type information is provided for all values.

    // Make sure "result" field always exists.
    if (jsonResult.isUndefined())
        response.insert("result", result.toString());
    else
        response.insert("result", jsonResult);

    const char* typeName = "";

    if (result.isUndefined())
    {
        typeName = "undefined";
    }
    else if (result.isCallable())
    {
        typeName = "function";
    }
    else if (result.isString())
    {
        typeName = "string";
    }
    else if (result.isBool())
    {
        typeName = "boolean";
    }
    else if (result.isNumber())
    {
        typeName = "number";
    }
    else if (result.isQObject())
    {
        typeName = "qobject";
        if (const auto o = result.toQObject())
            response.insert("metatype", o->metaObject()->className());
    }
    else if (result.isArray())
    {
        typeName = "array";
        response.insert("length", result.property("length").toInt());
    }
    else if (result.isObject())
    {
        typeName = "object";
        if (const auto t = resultVariant.typeName())
        {
            // QRect* does not expose its fields to the script,
            // so we just construct the JS objects with necessary fields.
            response.insert("metatype", t);
            if (resultVariant.type() == qMetaTypeId<QRect>())
                response.insert("result", rectToJson(resultVariant.toRect()));
            else if (resultVariant.type() == qMetaTypeId<QRectF>())
                response.insert("result", rectToJson(resultVariant.toRectF()));
        }
    }
    else if (result.isNull())
    {
        typeName = "null";
    }
    else if (result.isVariant())
    {
        typeName = "variant";
    }
    else if (result.isRegExp())
    {
        typeName = "regexp";
    }
    else if (result.isQMetaObject())
    {
        typeName = "qmetaobject";
    }
    else if (result.isDate())
    {
        typeName = "date";
    }
    else if (result.isError())
    {
        typeName = "error";
    }

    response.insert("type", typeName);

    return response;
}

QByteArray TestKit::screenshot(const char* format)
{
    QScreen* screen = QGuiApplication::primaryScreen();

    qApp->topLevelWindows().front();

    auto pixmap = screen->grabWindow(0);

    const auto size = pixmap.size();
    pixmap = pixmap.scaled(size / qApp->devicePixelRatio());

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, format);
    return bytes;
}

void TestKit::post(QJSValue object, QString eventName, QJSValue /* args */)
{
    ensureAsyncActionFinished();

    auto engine = qjsEngine(this);

    if (eventName == "QCloseEvent")
    {
        auto qobject = object.toQObject();
        if (!qobject)
        {
            engine->throwError(QString("Invalid object for event"));
            return;
        }

        auto event = new QCloseEvent();
        QCoreApplication::postEvent(qobject, event);
        return;
    }

    engine->throwError(QString("Event '%1' not implemented").arg(eventName));
}

QJSValue TestKit::bounds(QJSValue object)
{
    ensureAsyncActionFinished();

    return qjsEngine(this)->toScriptValue(utils::globalRect(object.toVariant()));
}

QJSValue TestKit::find(QJSValue properties)
{
    ensureAsyncActionFinished();

    QVariantList matchingObjects = utils::findAllObjects(properties);

    auto result = qjsEngine(this)->newArray(matchingObjects.size());
    for (int i = 0; i < matchingObjects.size(); ++i)
        result.setProperty(i, wrapQVariant(matchingObjects.at(i)));

    return result;
}

QJSValue TestKit::wrap(QJSValue object)
{
    ensureAsyncActionFinished();

    auto engine = qjsEngine(this);
    if (auto qobject = object.toQObject())
        return engine->toScriptValue(ObjectWrapper(qobject));
    const auto index = engine->fromScriptValue<QModelIndex>(object);
    if (index.isValid())
        return engine->toScriptValue(ModelIndexWrapper(index));
    return QJSValue();
}

void TestKit::mouse(QJSValue object, QJSValue parameters)
{
    ensureAsyncActionFinished();

    const bool hasCoordinates =
        parameters.property("x").isNumber() && parameters.property("y").isNumber();

    QPointF eventPos(parameters.property("x").toNumber(), parameters.property("y").toNumber());
    QWindow* window = nullptr;
    QPoint screenPos;

    if (object.isNull()) //< Screen.
    {
        screenPos = hasCoordinates ? eventPos.toPoint() : QCursor::pos();
    }
    else
    {
        const auto rect = utils::globalRect(object.toVariant(), &window);
        screenPos = hasCoordinates ? (rect.topLeft() + eventPos.toPoint()) : rect.center();
    }

    const auto buttonProp = parameters.property("button");
    Qt::MouseButton button = Qt::NoButton;

    if (buttonProp.toInt() == Qt::RightButton || buttonProp.toString() == "right")
        button = Qt::RightButton;
    else if (buttonProp.toInt() == Qt::LeftButton || buttonProp.toString() == "left")
        button = Qt::LeftButton;

    Qt::KeyboardModifiers modifiers(parameters.property("modifiers").toInt());
    if (!nx::build_info::isWindows())
        modifiers |= QGuiApplication::keyboardModifiers();

    if (!window)
    {
        window = QGuiApplication::topLevelAt(screenPos);
        if (!window)
            return;
    }

    utils::sendMouse(
        screenPos,
        parameters.property("type").toString(),
        /* mouseButton */ button,
        /* mouseButtons */ QGuiApplication::mouseButtons(),
        modifiers,
        window,
        parameters.property("native").toBool(),
        valueToPoint(parameters.property("pixelDelta")),
        valueToPoint(parameters.property("angleDelta")),
        parameters.property("inverted").toBool(),
        parameters.property("scrollDelta").toInt());
}

void TestKit::dragAndDrop(QJSValue object, QJSValue parameters)
{
    ensureAsyncActionFinished();

    const auto fromValue = parameters.property("from");
    const auto toValue = parameters.property("to");

    QPoint from(fromValue.property("x").toNumber(), fromValue.property("y").toNumber());
    QPoint to(toValue.property("x").toNumber(), toValue.property("y").toNumber());

    QWindow* window = nullptr;

    if (!object.isNull())
    {
        const auto objectRect = utils::globalRect(object.toVariant(), &window);
        from += objectRect.topLeft();
        to += objectRect.topLeft();
    }

    if (!window)
    {
        window = QGuiApplication::topLevelAt(from);
        if (!window)
            return;
    }

    auto doDragAndDrop =
        [this, from, to, window]
        {
            static constexpr auto kMouseInputDelay = std::chrono::milliseconds(150);

            QVector<QPoint> points;

            QPoint moveVector(to - from);
            for (int i = 0; i <= 10; ++i)
                points.push_back(from + moveVector * i / 10.0);

            utils::sendMouse(points.front(),
                "move",
                Qt::MouseButton::NoButton,
                Qt::MouseButtons(),
                Qt::KeyboardModifier::NoModifier,
                window,
                true);

            std::this_thread::sleep_for(kMouseInputDelay);

            utils::sendMouse(points.front(),
                "press",
                Qt::MouseButton::LeftButton,
                Qt::MouseButtons(Qt::MouseButton::LeftButton),
                Qt::KeyboardModifier::NoModifier,
                window,
                true);

            std::this_thread::sleep_for(kMouseInputDelay);

            for (const auto& point: points)
            {
                utils::sendMouse(point,
                    "move",
                    Qt::MouseButton::LeftButton,
                    Qt::MouseButtons(Qt::MouseButton::LeftButton),
                    Qt::KeyboardModifier::NoModifier,
                    window,
                    true);

                std::this_thread::sleep_for(kMouseInputDelay);
            }

            utils::sendMouse(points.back(),
                "release",
                Qt::MouseButton::LeftButton,
                Qt::MouseButtons(Qt::MouseButton::LeftButton),
                Qt::KeyboardModifier::NoModifier,
                window,
                true);

            m_dragAndDropActive = false;
        };

    m_dragAndDropActive = true;
    m_asyncActionThread = std::make_unique<std::thread>(doDragAndDrop);
}

void TestKit::keys(QJSValue object, QString keys, QString input)
{
    ensureAsyncActionFinished();

    input = input.toUpper();
    auto option = utils::KeyType;

    if (input == "PRESS")
        option = utils::KeyPress;
    else if (input == "RELEASE")
        option = utils::KeyRelease;

    utils::sendKeys(keys, object, option, QGuiApplication::queryKeyboardModifiers());
}

QJSValue TestKit::dump(QJSValue object, QJSValue withChildren)
{
    ensureAsyncActionFinished();

    auto engine = qjsEngine(this);
    QModelIndex index = engine->fromScriptValue<QModelIndex>(object);
    if (index.isValid())
    {
        return engine->toScriptValue(
            utils::dumpQModelIndex(index.model(), index, withChildren.toBool()));
    }

    auto wrap = engine->fromScriptValue<testkit::ModelIndexWrapper>(object);
    if (wrap.isValid())
    {
        return engine->toScriptValue(
            utils::dumpQModelIndex(wrap.index().model(), wrap.index(), withChildren.toBool()));
    }

    return engine->toScriptValue(utils::dumpQObject(object.toQObject(), withChildren.toBool()));
}

QJSValue TestKit::pick(int x, int y)
{
    ensureAsyncActionFinished();

    auto result = m_highlighter->pick({x, y}).toMap();
    if (result.isEmpty())
        return {};

    // Replace QRect with JSON.
    const QRect rect = result["rect"].toRect();
    QVariantMap rectObject;
    rectObject["x"] = rect.x();
    rectObject["y"] = rect.y();
    rectObject["width"] = rect.width();
    rectObject["height"] = rect.height();
    result["rect"] = rectObject;

    return qjsEngine(this)->toScriptValue(result);
}

TestKit* TestKit::instance()
{
    auto engine = appContext()->qmlEngine();

    return qobject_cast<testkit::TestKit*>(
            engine->globalObject().property(testkit::TestKit::kName).toQObject());
}

void TestKit::registerAction()
{
    registerDebugAction(
        "Toggle element highlight",
        [](QnWorkbenchContext*)
        {
            if (auto testkit = instance())
                testkit->m_highlighter->setEnabled(!testkit->m_highlighter->isEnabled());
        });
}

void TestKit::ensureAsyncActionFinished()
{
    if (m_asyncActionThread)
    {
        // Drag and drop won't be performed if GUI thread event loop will be blocked.
        NX_ASSERT(!m_dragAndDropActive,
            "TestKit method invoked while drag and drop operation in progress");

        m_asyncActionThread->join();
        m_asyncActionThread.reset();
    }
}

} // namespace nx::vms::client::desktop::testkit
