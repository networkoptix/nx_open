// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtQml/QJSValue>

namespace nx::vms::client::core::testkit {

class NX_VMS_CLIENT_CORE_API TestKit: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    TestKit(QObject* parent = nullptr);

public:
    /** Global object property name that points to the instance of this class. */
    static constexpr const char* kName = "testkit";

public:
    virtual ~TestKit();

    static void setup();

    /** Returns list of matching objects wrapped in QJSValue value. */
    Q_INVOKABLE QJSValue find(QJSValue properties);

    /** Screen rectangle of an object */
    Q_INVOKABLE QJSValue bounds(QJSValue object);

    /** Wraps QObject for accessing properties not exposed to scripting engine. */
    Q_INVOKABLE QJSValue wrap(QJSValue object);

    /**
     * Passes application events through a list of observers.
     * Observer callback is called if event name matches and receiver object parameters
     * also matches.
     * Supported events: QShowEvent, QHideEvent, QMouseEvent.
     *
     * <pre><code>
     * [
     *     {
     *         "event": <name of the event>,
     *         "match": <parameters of receiver object>,
     *         "callback": <callback function(matched_object)>
     *     },
     *     { ... },
     *     ...
     * ]
     * </code></pre>
     */
    Q_INVOKABLE void onEvent(QJSValue observers);

    /** Dumps QObject into JSON object */
    Q_INVOKABLE QJSValue dump(QJSValue object, QJSValue withChildren = false);

    /**
     * Takes screenshot of the application window and returns its image data.
     * Screenshot is scaled down by application pixel ratio so coordinates
     * inside the image are directly mapped to top level widgets.
     * Format name is one of the supported Qt formats for writing, see
     * https://doc.qt.io/qt-6/qpixmap.html#reading-and-writing-image-files
     */
    static QByteArray screenshot(const char* format);

    /**
     * Sends mouse event to the object. If the object is null the whole screen is used. Returns
     * new mouse buttons after sending mouse event(s).
     */
    Q_INVOKABLE void mouse(QJSValue object, QJSValue parameters);

    /**
     * Sends sequence of keyboard events to the object. If object is null the events are sent to
     * the active window. Input option may be "type", "press" or "release".
     */
    Q_INVOKABLE void keys(
        QJSValue object,
        QString keys,
        QString input = "type");

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QJSValue m_eventObservers;
};

} // namespace nx::vms::client::core::testkit
