// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QGuiApplication>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>

namespace nx::vms::client::desktop {

// Workaround for adding a signal to template class QmlDialogWithState.
class NX_VMS_CLIENT_DESKTOP_API QmlDialogWithStateNotifier: public QmlDialogWrapper
{
    using base_type = QmlDialogWrapper;

    Q_OBJECT

public:
    using base_type::base_type;

protected:
    static constexpr const char* kStateModifiedSignalName = "stateModified()";
    static constexpr const char* kStateAcceptedSignalName = "stateAccepted()";
    static constexpr const char* kStateAppliedSignalName = "stateApplied()";

signals:
    void stateModified();
    void stateAccepted();
    void stateApplied();
};

/**
 * Utility class for simplified dialog state handling. S must be a Q_GADGET representing the state
 * which the dialog allows to display and modify. Dialog QML object must contain all the properties
 * of the state and additional bool 'modified' property which indicates whether the state has been
 * modified inside the dialog.
 */
template <typename S, typename... Ts>
class QmlDialogWithState: public QmlDialogWithStateNotifier
{
    using base_type = QmlDialogWithStateNotifier;

    /** Copies Q_GADGET properties to Q_OBJECT. */
    static void stateToObject(const S& state, QObject* object)
    {
        auto meta = S::staticMetaObject;

        for (int i = 0; i < meta.propertyCount(); ++i)
        {
            const auto& property = meta.property(i);
            NX_ASSERT(object->metaObject()->indexOfProperty(property.name()) != -1,
                "No such property '%1' in %2", property.name(), object);
            object->setProperty(property.name(), property.readOnGadget(&state));
        }
    }

    /** Creates Q_GADGET with properties obtained from Q_OBJECT. */
    static S objectToState(QObject* object)
    {
        S state;
        auto meta = S::staticMetaObject;

        for (int i = 0; i < meta.propertyCount(); ++i)
        {
            const auto& property = meta.property(i);
            property.writeOnGadget(&state, object->property(property.name()));
        }
        return state;
    }

    /**
     * Merges states when the state is externally updated. All properties that have not been
     * changed by the user (original -> edited) are replaced with the properties of incoming state.
     */
    static S mergeStates(const S& original, const S& edited, const S& incoming)
    {
        S result;

        auto meta = S::staticMetaObject;

        for (int i = 0; i < meta.propertyCount(); ++i)
        {
            const auto& property = meta.property(i);
            const auto originalValue = property.readOnGadget(&original);
            const auto editedValue = property.readOnGadget(&edited);
            property.writeOnGadget(&result,
                property.readOnGadget(originalValue == editedValue ? &incoming : &edited));
        }

        return result;
    }

    /**
     * Connects all change notification signals of the object properties (which are part of state)
     * to the target's signal.
     */
    static void subscribeToChanges(QObject* object, QObject* target, const char* signal)
    {
        auto meta = S::staticMetaObject;

        for (int i = 0; i < meta.propertyCount(); ++i)
        {
            const auto& property = meta.property(i);
            const QMetaObject* metaObject = object->metaObject();

            NX_ASSERT(metaObject->indexOfProperty(property.name()) != -1,
                "No such property '%1' in %2", property.name(), object);

            const auto objectProperty =
                metaObject->property(metaObject->indexOfProperty(property.name()));

            if (objectProperty.hasNotifySignal())
            {
                const QMetaObject* targetMetaObject = target->metaObject();
                const int index = targetMetaObject->indexOfSignal(signal);
                if (NX_ASSERT(index >= 0, "Signal '%1' must exist in %2", signal, target))
                {
                    QObject::connect(
                        object, objectProperty.notifySignal(),
                        target, targetMetaObject->method(index));
                }
            }
        }
    }

    static constexpr auto kModifiedPropertyName = "modified";

public:
    QmlDialogWithState(QWidget* parent, const QString& qmlSourceFile, QQmlEngine* engine = nullptr):
        base_type(
            engine ? engine : appContext()->qmlEngine(),
            QUrl(qmlSourceFile),
            /*initialProperties*/ {},
            parent ? parent->window()->windowHandle() : nullptr)
    {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &QObject::deleteLater);

        connect(this, &QmlDialogWithStateNotifier::stateModified, this,
            [this]
            {
                setModified(originalState() != currentState());
            });

        const auto onInitialized = 
            [this]
            {
                auto rootObject = rootObjectHolder()->object();

                NX_ASSERT(rootObject->metaObject()->indexOfProperty(kModifiedPropertyName) != -1,
                    "No such property '%1' in %2",
                    kModifiedPropertyName,
                    rootObject->metaObject());

                subscribeToChanges(rootObject, this, kStateModifiedSignalName);

                if (rootObject->metaObject()->indexOfSignal(kStateAcceptedSignalName))
                    connect(rootObject, SIGNAL(stateAccepted()), this, SIGNAL(stateAccepted()));
                if (rootObject->metaObject()->indexOfSignal(kStateAppliedSignalName))
                    connect(rootObject, SIGNAL(stateApplied()), this, SIGNAL(stateApplied()));
            };

        if (!rootObjectHolder()->object())
            connect(this, &base_type::initialized, onInitialized);
        else
            onInitialized();

        const auto applyState =
            [this]
            {
                saveState(currentState());
            };

        connect(this, &base_type::stateAccepted,
            [this, applyState]
            {
                m_acceptOnSuccess = true;
                applyState();
            });

        connect(this, &base_type::stateApplied,
            [this, applyState]
            {
                m_acceptOnSuccess = false;
                applyState();
            });
    }

protected:
    /**
     * Updates 'modified' property of the dialog.
     */
    void setModified(bool value = true)
    {
        rootObjectHolder()->object()->setProperty(kModifiedPropertyName, value);
    }

    /**
     * Creates new dialog state from provided arguments. It is called when
     * createStateFrom() or updateStateFrom() is called.
     */
    virtual S createState(const Ts&... args) = 0;

    /**
     * Applies the changes in the dialog. If the changes are successfully saved call
     * saveStateComplete() with the provided state.
     */
    virtual void saveState(const S& state) = 0;

    /**
     * Updates dialog state with changes coming from outside of the dialog.
     */
    void updateStateFrom(const Ts&... args)
    {
        const auto incomingState = createState(args...);
        const auto editedState = currentState();
        auto state = mergeStates(m_originalState, editedState, incomingState);

        m_originalState = incomingState;
        stateToObject(state, rootObjectHolder()->object());

        setModified(m_originalState != state);
    }

    /**
     * Initializes dialog from a set of arguments.
     */
    void createStateFrom(const Ts&... args)
    {
        auto state = createState(args...);

        m_originalState = state;
        stateToObject(state, rootObjectHolder()->object());

        setModified(false);
    }

    /**
     * Notifies the dialog that the changes have been successfully applied.
     */
    void saveStateComplete(const S& state)
    {
        m_originalState = state;
        setModified(false);

        if (m_acceptOnSuccess)
            accept(); // Will emit accepted() signal.
        else
            emit applied();
    }

    /**
     * Checks whether dialog state is modified.
     */
    bool isModified() const
    {
        return originalState() != currentState();
    }

    /**
     * Returns original state of the dialog loaded via createStateFrom() or updateStateFrom().
     */
    const S& originalState() const { return m_originalState; }

    /**
     * Returns current state of the dialog.
     */
    S currentState() const { return objectToState(rootObjectHolder()->object()); }

private:
    S m_originalState;
    bool m_acceptOnSuccess = false;
};

} // namespace nx::vms::client::desktop
