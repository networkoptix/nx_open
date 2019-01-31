#pragma once

class QnLabelFocusListener: public QObject
{
public:
    QnLabelFocusListener(QObject* parent = nullptr);
protected:
    virtual bool eventFilter(QObject* obj, QEvent* event) override;

};
