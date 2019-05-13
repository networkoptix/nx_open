#pragma once

#include <QtCore/QObject>

class ComboboxWheelFilter : public QObject
{
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
};
