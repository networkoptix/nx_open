// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QDoubleValidator>

namespace nx::vms::client::core {

class IntValidator: public QIntValidator
{
    Q_OBJECT
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale)
    Q_PROPERTY(int lowest READ lowest CONSTANT) //< Minimum possible int value.
    Q_PROPERTY(int highest READ highest CONSTANT) //< Maximum possible int value.

public:
    explicit IntValidator(QObject* parent = nullptr);
    static void registerQmlType();

private:
    static int lowest();
    static int highest();
};

class DoubleValidator: public QDoubleValidator
{
    Q_OBJECT
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale)
    Q_PROPERTY(double lowest READ lowest CONSTANT) //< Minimum possible double value.
    Q_PROPERTY(double highest READ highest CONSTANT) //< Maximum possible double value.

public:
    explicit DoubleValidator(QObject* parent = nullptr);
    static void registerQmlType();

private:
    static double lowest();
    static double highest();
};

} // namespace nx::vms::client::core
