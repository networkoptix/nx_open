// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_dialog_wrapper.h"

#include <QtCore/QEventLoop>
#include <QtGui/QWindow>
#include <QtQml/QQmlComponent>
#include <QtWidgets/QWidget>

#include <client_core/client_core_module.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

class QmlDialogWrapper::Private: public QObject
{
public:
    QWindow* transientParent = nullptr;
    std::unique_ptr<QQuickWindow> window = nullptr;
    QmlDialogWrapper* q = nullptr;
    QEventLoop eventLoop;
    QQmlComponent* qmlComponent = nullptr;
    QVariantMap initialProperties;
    core::QmlPropertyBase::ObjectHolder rootObjectHolder;
    QUrl source;
    bool done = false;
    bool shownMaximized = false;
    bool restoreLastPositionWhenOpened = false;
    QRect lastGeometry;

public:
    Private(QmlDialogWrapper* q);
    void handleStatusChanged(QQmlComponent::Status status);

    void restoreGeometryIfNecessary();

    virtual bool eventFilter(QObject* object, QEvent* event) override;
};

QmlDialogWrapper::Private::Private(QmlDialogWrapper* q):
    q(q)
{
}

void QmlDialogWrapper::Private::handleStatusChanged(QQmlComponent::Status status)
{
    rootObjectHolder.setObject(nullptr);
    window.reset();

    switch (status)
    {
        case QQmlComponent::Null:
        case QQmlComponent::Loading:
            return;

        case QQmlComponent::Error:
            for (const QQmlError& error: qmlComponent->errors())
                NX_WARNING(this, error.toString());
            return;

        case QQmlComponent::Ready:
            break;
    }

    auto properties = initialProperties;
    properties["visible"] = false; //< We want to control window visibility manually.
    if (transientParent)
        properties["transientParent"] = QVariant::fromValue(transientParent);

    QObject* object = qmlComponent->createWithInitialProperties(properties);
    if (!NX_ASSERT(object, "QML object cannot be created."))
        return;

    window.reset(qobject_cast<QQuickWindow*>(object));
    if (!NX_ASSERT(window, "The created object is not a QQuickWindow. It cannot be shown!"))
    {
        delete object;
        return;
    }

    window->setProperty("qmlDialogWrapper_sourceUrl", source.toString()); //< For autotesting.

    if (object->metaObject()->indexOfSignal("accepted()") >= 0)
        connect(object, SIGNAL(accepted()), q, SLOT(accept()));
    if (object->metaObject()->indexOfSignal("rejected()") >= 0)
        connect(object, SIGNAL(rejected()), q, SLOT(reject()));
    if (object->metaObject()->indexOfSignal("applied()") >= 0)
        connect(object, SIGNAL(applied()), q, SIGNAL(applied()));

    rootObjectHolder.setObject(window.get());
    window->installEventFilter(this);
    emit q->initialized();
}

void QmlDialogWrapper::Private::restoreGeometryIfNecessary()
{
    if (restoreLastPositionWhenOpened && !window->isVisible() && lastGeometry.isValid())
        window->setGeometry(lastGeometry);
}

bool QmlDialogWrapper::Private::eventFilter(QObject* /*object*/, QEvent* event)
{
    if (event->type() == QEvent::Close)
        lastGeometry = window->geometry();

    return false;
}

//-------------------------------------------------------------------------------------------------

QmlDialogWrapper::QmlDialogWrapper():
    QmlDialogWrapper(qnClientCoreModule->mainQmlEngine(), QUrl(), {}, (QWindow*) nullptr)
{
}

QmlDialogWrapper::QmlDialogWrapper(QQmlEngine* engine,
    const QUrl& sourceUrl,
    const QVariantMap& initialProperties,
    QWindow* parent)
    :
    d(new Private(this))
{
    d->transientParent = parent;
    d->qmlComponent = new QQmlComponent(engine);
    d->initialProperties = initialProperties;

    connect(
        d->qmlComponent, &QQmlComponent::statusChanged, d.get(), &Private::handleStatusChanged);

    connect(this, &QmlDialogWrapper::accepted, this, [this]() { emit done(true); });
    connect(this, &QmlDialogWrapper::rejected, this, [this]() { emit done(false); });

    setSource(sourceUrl);
}

QmlDialogWrapper::QmlDialogWrapper(
    const QUrl& sourceUrl, const QVariantMap& initialProperties, QWindow* parent):
    QmlDialogWrapper(qnClientCoreModule->mainQmlEngine(), sourceUrl, initialProperties, parent)
{
}

QmlDialogWrapper::QmlDialogWrapper(QQmlEngine* engine,
    const QUrl& sourceUrl,
    const QVariantMap& initialProperties,
    QWidget* parent)
    :
    QmlDialogWrapper(engine, sourceUrl, initialProperties, parent->window()->windowHandle())
{
}

QmlDialogWrapper::QmlDialogWrapper(
    const QUrl& sourceUrl, const QVariantMap& initialProperties, QWidget* parent):
    QmlDialogWrapper(sourceUrl, initialProperties, parent->window()->windowHandle())
{
}

QmlDialogWrapper::~QmlDialogWrapper()
{
}

QUrl QmlDialogWrapper::source() const
{
    return d->source;
}

void QmlDialogWrapper::setSource(const QUrl& source)
{
    if (d->source == source)
        return;

    d->source = source;
    emit sourceChanged();

    d->qmlComponent->loadUrl(source);
}

void QmlDialogWrapper::setData(const QByteArray& data, const QUrl& url)
{
    d->qmlComponent->setData(data, url);
}

QVariantMap QmlDialogWrapper::initialProperties() const
{
    return d->initialProperties;
}

void QmlDialogWrapper::setInitialProperties(const QVariantMap& initialProperties)
{
    if (d->initialProperties == initialProperties)
        return;

    d->initialProperties = initialProperties;
    emit initialPropertiesChanged();
}

QWindow* QmlDialogWrapper::transientParent() const
{
    return d->window ? d->window->transientParent() : d->transientParent;
}

void QmlDialogWrapper::setTransientParent(QWindow* window)
{
    // Update both the initial value and the actual property of the window.
    if (d->transientParent == window && (!d->window || d->window->transientParent() == window))
        return;

    d->transientParent = window;
    emit transientParentChanged();

    if (d->window)
        d->window->setTransientParent(window);
}

void QmlDialogWrapper::setTransientParent(QWidget* widget)
{
    setTransientParent(widget ? widget->window()->windowHandle() : nullptr);
}

QQuickWindow* QmlDialogWrapper::window() const
{
    return d->window.get();
}

void QmlDialogWrapper::setMaximized(bool value)
{
    d->shownMaximized = value;

    if (d->window && d->window->isVisible())
        d->window->setWindowState(value ? Qt::WindowMaximized : Qt::WindowNoState);
}

bool QmlDialogWrapper::restoreLastPositionWhenOpened() const
{
    return d->restoreLastPositionWhenOpened;
}

void QmlDialogWrapper::setRestoreLastPositionWhenOpened(bool value)
{
    if (d->restoreLastPositionWhenOpened == value)
        return;

    d->restoreLastPositionWhenOpened = value;
    emit restoreLastPositionWhenOpenedChanged();
}

bool QmlDialogWrapper::exec(Qt::WindowModality modality)
{
    if (!d->window)
        return false;

    if (!NX_ASSERT(!d->eventLoop.isRunning(), "Dialog is already opened!"))
        return false;

    d->done = false;

    d->restoreGeometryIfNecessary();

    const auto oldModality = d->window->modality();
    d->window->setModality(modality);

    if (d->shownMaximized)
        d->window->showMaximized();
    else
        d->window->show();

    const int result = d->eventLoop.exec();
    d->window->setModality(oldModality);

    // For some reason if a dialog is closed by the [X] button or via the system menu,
    // MS Windows passes activity to another application. So this workaround is required.
    if (d->window->transientParent())
        d->window->transientParent()->requestActivate();

    return result == 0;
}

void QmlDialogWrapper::open()
{
    if (!d->window)
        return;

    d->done = false;

    d->restoreGeometryIfNecessary();

    const auto oldModality = d->window->modality();
    d->window->setModality(Qt::NonModal);

    if (d->shownMaximized)
        d->window->showMaximized();
    else
        d->window->show();

    d->window->setModality(oldModality);
}

void QmlDialogWrapper::raise()
{
    if (d->window && d->window->isVisible())
    {
        d->window->raise();
        d->window->requestActivate();
    }
}

void QmlDialogWrapper::accept()
{
    if (!d->window)
        return;

    if (d->done)
        return;

    if (d->window->metaObject()->indexOfMethod("accept") >= 0)
    {
        QMetaObject::invokeMethod(d->window.get(), "accept");
        if (!d->window->isVisible() && d->eventLoop.isRunning())
        {
            d->done = true;
            d->eventLoop.exit();
        }
    }
    else
    {
        // The wrapped dialog can emit accepted/rejected when closed. Mark the dialog as done to
        // avoid extra reject() call.
        d->done = true;

        bool closed = true;

        if (d->window->isVisible())
            closed = d->window->close();

        if (!closed)
            return;

        if (d->eventLoop.isRunning())
            d->eventLoop.exit();

        emit accepted();
    }
}

void QmlDialogWrapper::reject()
{
    if (!d->window)
        return;

    if (d->done)
        return;

    if (d->window->metaObject()->indexOfMethod("reject") >= 0)
    {
        QMetaObject::invokeMethod(d->window.get(), "reject");
        if (!d->window->isVisible() && d->eventLoop.isRunning())
        {
            d->done = true;
            d->eventLoop.exit(1);
        }
    }
    else
    {
        // The wrapped dialog can emit accepted/rejected when closed. Mark the dialog as done to
        // avoid extra reject() call.
        d->done = true;

        if (d->window->isVisible())
        {
            if (!d->window->close()) //< If close() didn't work at least hide the window.
                d->window->hide();
        }
        if (d->eventLoop.isRunning())
            d->eventLoop.exit(1);

        emit rejected();
    }
}

core::QmlPropertyBase::ObjectHolder* QmlDialogWrapper::rootObjectHolder() const
{
    return &d->rootObjectHolder;
}

} // namespace nx::vms::client::desktop
