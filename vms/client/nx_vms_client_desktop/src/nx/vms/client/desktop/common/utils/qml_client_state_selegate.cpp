// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_client_state_selegate.h"

#include <QtQml/QtQml>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>

namespace nx::vms::client::desktop {

class QmlClientStateDelegate::Delegate: public ClientStateDelegate
{
    QmlClientStateDelegate* const m_main;

public:
    Delegate(QmlClientStateDelegate* main): m_main(main) {}

    virtual bool loadState(const DelegateState& state, SubstateFlags flags,
        const StartupParameters& /*params*/) override
    {
        if (!NX_ASSERT(m_main) || !flags.testFlag(requiredFlag()))
            return false;

        emit m_main->stateAboutToBeLoaded({});

        forEachQmlProperty(
            [this, &state](const QMetaProperty& prop)
            {
                if (const auto it = state.find(prop.name()); it != state.cend())
                {
                    prop.write(m_main, it->toVariant());
                }
            });

        emit m_main->stateLoaded({});
        return true;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (!NX_ASSERT(m_main) || !flags.testFlag(requiredFlag()))
            return;

        emit m_main->stateAboutToBeSaved({});

        forEachQmlProperty(
            [this, state](const QMetaProperty& prop)
            {
                (*state)[prop.name()] = QJsonValue::fromVariant(prop.read(m_main));
            });

        emit m_main->stateSaved({});
    }

    Substate requiredFlag() const
    {
        return m_main->systemDependent()
            ? Substate::systemSpecificParameters
            : Substate::systemIndependentParameters;
    }

private:
    void forEachQmlProperty(std::function<void(const QMetaProperty&)> callback)
    {
        const auto metaObject = m_main->metaObject();
        const auto firstUserProperty = QmlClientStateDelegate::staticMetaObject.propertyCount();

        for (int i = firstUserProperty; i < metaObject->propertyCount(); ++i)
            callback(metaObject->property(i));
    }
};

QmlClientStateDelegate::QmlClientStateDelegate(QObject* parent):
    QObject(parent),
    m_delegate(std::make_shared<Delegate>(this))
{
}

QmlClientStateDelegate::~QmlClientStateDelegate()
{
    if (!m_name.isEmpty())
        appContext()->clientStateHandler()->unregisterDelegate(m_name);
}

bool QmlClientStateDelegate::systemDependent() const
{
    return m_systemDependent;
}

void QmlClientStateDelegate::setSystemDependent(bool value)
{
    m_systemDependent = value;
}

QString QmlClientStateDelegate::name() const
{
    return m_name;
}

void QmlClientStateDelegate::setName(const QString& value)
{
    const auto trimmedValue = value.trimmed();
    if (m_name == trimmedValue)
        return;

    if (!m_name.isEmpty())
        appContext()->clientStateHandler()->unregisterDelegate(m_name);

    m_name = trimmedValue;
    if (!m_name.isEmpty())
        appContext()->clientStateHandler()->registerDelegate(m_name, m_delegate);
}

void QmlClientStateDelegate::reportStatistics(const QString& name, const QString& value)
{
    m_delegate->reportStatistics(name, value);
}

void QmlClientStateDelegate::registerQmlType()
{
    qmlRegisterType<QmlClientStateDelegate>("nx.vms.client.desktop", 1, 0, "ClientStateDelegate");
}

} // namespace nx::vms::client::desktop
