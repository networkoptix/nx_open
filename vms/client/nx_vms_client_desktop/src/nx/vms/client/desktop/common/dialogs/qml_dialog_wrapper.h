// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariant>

#include <nx/vms/client/desktop/utils/qml_property.h>

class QEventLoop;
class QQmlEngine;
class QWindow;
class QWidget;

namespace nx::vms::client::desktop {

/**
 * A wrapper which proxies accepted()/rejected() signals and accept()/reject() slots between C++
 * and a QML Dialog Window.
 */
class NX_VMS_CLIENT_DESKTOP_API QmlDialogWrapper: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QVariantMap initialProperties READ initialProperties WRITE setInitialProperties
        NOTIFY initialPropertiesChanged)
    Q_PROPERTY(QWindow* transientParent READ transientParent WRITE setTransientParent
        NOTIFY transientParentChanged)
    Q_PROPERTY(bool restoreLastPositionWhenOpened READ restoreLastPositionWhenOpened
        WRITE setRestoreLastPositionWhenOpened NOTIFY restoreLastPositionWhenOpenedChanged)

public:
    QmlDialogWrapper();
    QmlDialogWrapper(QQmlEngine* engine,
        const QUrl& sourceUrl,
        const QVariantMap& initialProperties = {},
        QWindow* parent = nullptr);
    QmlDialogWrapper(const QUrl& sourceUrl,
        const QVariantMap& initialProperties = {},
        QWindow* parent = nullptr);
    QmlDialogWrapper(QQmlEngine* engine,
        const QUrl& sourceUrl,
        const QVariantMap& initialProperties,
        QWidget* parent);
    QmlDialogWrapper(const QUrl& sourceUrl,
        const QVariantMap& initialProperties,
        QWidget* parent);
    virtual ~QmlDialogWrapper() override;

    /** Source QML URL. */
    QUrl source() const;
    void setSource(const QUrl& source);

    void setData(const QByteArray& data, const QUrl& url);

    /** Initial properties for the wrapped dialog. */
    QVariantMap initialProperties() const;
    void setInitialProperties(const QVariantMap& initialProperties);

    /** Parent window of the wrapped dialog. */
    QWindow* transientParent() const;
    void setTransientParent(QWindow* window);
    void setTransientParent(QWidget* widget);

    /** Get the wrapped dialog window. */
    QQuickWindow* window() const;

    /** Change dialog maximized state now or when shown if currently it isn't visible. */
    void setMaximized(bool value);

    /** If true, the dialog will restore its previous position when opened after being closed. */
    bool restoreLastPositionWhenOpened() const;
    void setRestoreLastPositionWhenOpened(bool value = true);

signals:
    void initialized();
    void sourceChanged();
    void initialPropertiesChanged();
    void transientParentChanged();
    void restoreLastPositionWhenOpenedChanged();

    /** Emitted when the wrapped dialog changes either accepted or rejected. */
    void done(bool accepted);
    /** Emitted when the wrapped dialog changes are accepted. */
    void accepted();
    /** Emitted when the wrapped dialog changes are rejected. */
    void rejected();
    /** Emitted when the wrapped dialog changes are applied. */
    void applied();

public slots:
    /** Open the wrapped dialog and wait until it's closed. */
    bool exec(Qt::WindowModality modality = Qt::WindowModal);

    /** Open the wrapped dialog. */
    void open();

    /** Raise the wrapped dialog to the top. */
    void raise();

    /** Close the wrapped dialog with result `true`. */
    void accept();

    /** Close the wrapped dialog with result `false`. */
    void reject();

protected:
    core::QmlPropertyBase::ObjectHolder* rootObjectHolder() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
