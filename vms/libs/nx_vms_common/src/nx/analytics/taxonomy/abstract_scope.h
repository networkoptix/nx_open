// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::analytics::taxonomy {

class AbstractEngine;
class AbstractGroup;

class NX_VMS_COMMON_API AbstractScope: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::analytics::taxonomy::AbstractEngine* engine READ engine CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractGroup* group READ group CONSTANT)
    Q_PROPERTY(bool empty READ isEmpty CONSTANT)

public:
    AbstractScope(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractScope() {}

    virtual AbstractEngine* engine() const = 0;

    virtual AbstractGroup* group() const = 0;

    virtual bool isEmpty() const = 0;
};

} // namespace nx::analytics::taxonomy
