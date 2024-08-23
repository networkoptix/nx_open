// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "testkit.h"

#include <QtCore/QBuffer>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickWindow>

#include <nx/build_info.h>
#include <nx/vms/client/core/application_context.h>

#include "utils.h"

namespace nx::vms::client::core::testkit {

namespace {

QJSValue wrapQObject(QObject* obj)
{
    auto engine = appContext()->qmlEngine();

    const auto ownership = QQmlEngine::objectOwnership(obj);
    QQmlEngine::setObjectOwnership(obj, ownership);
    return engine->newQObject(obj);
}

QJSValue wrapQVariant(QVariant value)
{
    if (auto object = qvariant_cast<QObject*>(value))
        return wrapQObject(object);

    auto engine = appContext()->qmlEngine();
    return engine->toScriptValue(value);
}

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

TestKit::TestKit(QObject* parent): base_type(parent)
{
    qApp->installEventFilter(this);
}

TestKit::~TestKit()
{
    qApp->removeEventFilter(this);
}

void TestKit::setup()
{
    auto engine = appContext()->qmlEngine();
    engine->installExtensions(QJSEngine::AllExtensions);

    // Create a new instance and pass the ownership to the engine.
    auto testkit = new TestKit();
    engine->globalObject().setProperty(kName, engine->newQObject(testkit));
}

QJSValue TestKit::find(QJSValue properties)
{
    auto engine = appContext()->qmlEngine();

    QVariantList matchingObjects = utils::findAllObjects(properties);

    auto result = engine->newArray(matchingObjects.size());
    for (int i = 0; i < matchingObjects.size(); ++i)
        result.setProperty(i, wrapQVariant(matchingObjects.at(i)));

    return result;
}

QJSValue TestKit::bounds(QJSValue object)
{
    auto engine = appContext()->qmlEngine();
    return engine->toScriptValue(utils::globalRect(object.toVariant()));
}

QJSValue TestKit::wrap(QJSValue object)
{
    return object;
}

void TestKit::onEvent(QJSValue observers)
{
    m_eventObservers = observers;
}

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

QByteArray TestKit::screenshot(const char* format)
{
    const auto windowList = qApp->topLevelWindows();

    auto window = qobject_cast<QQuickWindow*>(windowList.front());
    if (!window)
        return {};

    auto image = window->grabWindow();

    const auto size = image.size();
    image = image.scaled(size / qApp->devicePixelRatio());

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format);
    return bytes;
}

QJSValue TestKit::dump(QJSValue object, QJSValue withChildren)
{
    auto engine = appContext()->qmlEngine();
    return engine->toScriptValue(utils::dumpQObject(object.toQObject(), withChildren.toBool()));
}

void TestKit::mouse(QJSValue object, QJSValue parameters)
{
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
        const auto rect = utils::globalRect(object.toVariant());
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

    core::testkit::utils::sendMouse(
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

void TestKit::keys(
    QJSValue object,
    QString keys,
    QString input)
{
    input = input.toUpper();
    auto option = utils::KeyType;

    if (input == "PRESS")
        option = utils::KeyPress;
    else if (input == "RELEASE")
        option = utils::KeyRelease;

    utils::sendKeys(keys, object.toQObject(), option, QGuiApplication::queryKeyboardModifiers());
}

} // namespace nx::vms::client::core::testkit
