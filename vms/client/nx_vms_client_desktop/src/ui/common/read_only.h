// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_READ_ONLY_H
#define QN_READ_ONLY_H

class QWidget;

void setReadOnly(QWidget *widget, bool readOnly);
bool isReadOnly(const QWidget *widget);

#endif // QN_READ_ONLY_H
