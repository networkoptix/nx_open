// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <QtCore/QObject>
#include <QtQml/QJSValue>

namespace nx::vms::client::desktop::testkit {

class Highlighter;

/**
 * Provides UI testing capabilities via scripting engine.
 */
class TestKit: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    /** Global object property name that points to the instance of this class. */
    static constexpr const char* kName = "testkit";

    TestKit(QObject* parent = nullptr);
    virtual ~TestKit();

    /** Adds a new class instance to script global object. */
    static void setup(QJSEngine* engine);

    /**
     * Executes script source code and provides result as JSON.
     * The execution takes place in main UI thread, so don't use this for long computations.
     * Saving the actual result value so it won't be garbage collected is the responsibility
     * of the caller.
     *
     * This should be the only entry point for performing test requests.
     *
     * @returns Execution result JSON object:
     * <pre><code>
     * {
     *     "result": <JSON representation of result>,
     *     "type": <string name of JS type number, array, qobject etc.>,
     *     "length": <number of elements in array, only if "type" == "array">,
     *     "metatype": <Qt metattype name, only when "type" is "qobject" or "object">,
     *     "error": <greater than 0 only if script execution fails, undefined otherwise>,
     *     "errorString": <string decription of the error, only if script execution fails>
     * }
     * </code></pre>
     */
    QJsonObject execute(const QString& source);

    /**
     * Takes screenshot of the primary screen and returns its image data.
     * Screenshot is scaled down by application pixel ratio so coordinates
     * inside the image are directly mapped to top level widgets.
     * Format name is one of the supported Qt formats for writing, see
     * https://doc.qt.io/qt-6/qpixmap.html#reading-and-writing-image-files
     */
    static QByteArray screenshot(const char* format);

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

    /**
     * Posts Qt event to specified object.
     * Supported events: QCloseEvent.
     */
    Q_INVOKABLE void post(QJSValue object, QString eventName, QJSValue args = QJSValue());

    /** Returns list of matching objects wrapped in QJSValue value. */
    Q_INVOKABLE QJSValue find(QJSValue properties);

    /** Screen rectangle of an object */
    Q_INVOKABLE QJSValue bounds(QJSValue object);

    /** Wraps QObject for accessing properties not exposed to scripting engine. */
    Q_INVOKABLE QJSValue wrap(QJSValue object);

    /**
     * Sends mouse event to the object. If the object is null the whole screen is used. Returns
     * new mouse buttons after sending mouse event(s).
     */
    Q_INVOKABLE void mouse(QJSValue object, QJSValue parameters);

    /**
     * Emulates drag and drop action by performing the sequence of mouse input events. Mouse input
     * emulation is executed asynchronously in the non-GUI thread, if any other TestKit method will
     * be invoked while drag and drop is performed this will block the GIU thread until the
     * sequence ends, and drag and drop wouldn't happen. Thus it's required to place at least
     * 3 seconds delay after invoking this method in the test script to be sure that it will work
     * correctly.
     */
    Q_INVOKABLE void dragAndDrop(QJSValue object, QJSValue parameters);

    /**
     * Sends sequence of keyboard events to the object. If object is null the events are sent to
     * the active window. Input option may be "type", "press" or "release".
     */
    Q_INVOKABLE void keys(
        QJSValue object,
        QString keys,
        QString input = "type");

    /** Dumps QObject into JSON object */
    Q_INVOKABLE QJSValue dump(QJSValue object, QJSValue withChildren=false);

    /** Returns visible element with the smallest size at specified screen coordinates. */
    Q_INVOKABLE QJSValue pick(int x, int y);

    /** Enables hightlight for UI elements under cursor. */
    void setHighlightEnabled(bool enabled = true);

    /** A pointer to created TestKit instance. */
    static TestKit* instance();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    /** Exposes QObject to script without changing its ownership. */
    QJSValue wrapQObject(QObject* obj);

    /** Exposes QObject stored in QVariant to script without changing its ownership. */
    QJSValue wrapQVariant(QVariant value);

    /**
     * Blocks the current thread until the asyncronious operation, e.g drag and drop finishes its
     * execution.
     */
    void ensureAsyncActionFinished();

    QJSValue m_eventObservers;

    std::unique_ptr<Highlighter> m_highlighter;
    std::unique_ptr<std::thread> m_asyncActionThread;
    std::atomic<bool> m_dragAndDropActive = false;
};

} // namespace nx::vms::client::desktop::testkit
