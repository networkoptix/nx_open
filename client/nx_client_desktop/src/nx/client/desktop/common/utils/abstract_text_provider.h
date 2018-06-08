#pragma once

#include <QtCore/QObject>

namespace nx {
namespace client {
namespace desktop {

class AbstractTextProvider: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject; //< Forward constructors.

    virtual QString text() const = 0;

signals:
    void textChanged(const QString& value);
};

} // namespace desktop
} // namespace client
} // namespace nx
