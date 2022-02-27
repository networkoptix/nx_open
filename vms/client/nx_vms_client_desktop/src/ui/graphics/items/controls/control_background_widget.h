// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/graphics/items/generic/framed_widget.h>

class QnControlBackgroundWidget: public QnFramedWidget {
    Q_OBJECT
    typedef QnFramedWidget base_type;

public:
    QnControlBackgroundWidget(QGraphicsItem *parent = nullptr);

};
