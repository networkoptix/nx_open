// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

class QnLabelFocusListener: public QObject
{
public:
    QnLabelFocusListener(QObject* parent = nullptr);
protected:
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

};
