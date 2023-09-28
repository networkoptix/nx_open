// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <QtWidgets/QLineEdit>

class QnIpLineEdit: public QLineEdit
{
    Q_OBJECT
typedef QLineEdit base_type;

public:
    explicit QnIpLineEdit(QWidget* parent=0);
    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void focusOutEvent(QFocusEvent* event) override;
};
