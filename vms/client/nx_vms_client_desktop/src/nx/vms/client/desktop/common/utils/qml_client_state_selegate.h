// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

namespace nx::vms::client::desktop {

/**
 * A client state delegate intended to be instantiated from QML (as ClientStateDelegate component).
 * It saves and loads all its user-declared properties.
 * It can be either system-dependent or system-independent: to handle both cases you need
 * two instances of ClientStateDelegate with different names.
 */
class QmlClientStateDelegate: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(bool systemDependent READ systemDependent WRITE setSystemDependent)

public:
    QmlClientStateDelegate(QObject* parent = nullptr);
    virtual ~QmlClientStateDelegate() override;

    bool systemDependent() const;
    void setSystemDependent(bool value);

    QString name() const;
    void setName(const QString& value);

    Q_INVOKABLE void reportStatistics(const QString& name, const QString& value);

    static void registerQmlType();

signals:
    void stateAboutToBeLoaded(QPrivateSignal);
    void stateLoaded(QPrivateSignal);
    void stateAboutToBeSaved(QPrivateSignal);
    void stateSaved(QPrivateSignal);

private:
    QString m_name;
    bool m_systemDependent = false;

private:
    class Delegate;
    std::shared_ptr<Delegate> m_delegate;
};

} // namespace nx::vms::client::desktop
