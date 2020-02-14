#include <QtGui/QDoubleValidator>

namespace nx::vms::client::core {

class IntValidator: public QIntValidator
{
    Q_OBJECT
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale)

public:
    explicit IntValidator(QObject* parent = nullptr);
    static void registerQmlType();
};

class DoubleValidator: public QDoubleValidator
{
    Q_OBJECT
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale)

public:
    explicit DoubleValidator(QObject* parent = nullptr);
    static void registerQmlType();
};

} // namespace nx::vms::client::core
