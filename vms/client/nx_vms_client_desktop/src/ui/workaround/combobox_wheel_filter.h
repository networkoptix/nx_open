// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

class ComboboxWheelFilter : public QObject
{
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
};
