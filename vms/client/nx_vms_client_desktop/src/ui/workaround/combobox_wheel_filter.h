#pragma once

class ComboboxWheelFilter : public QObject
{
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
};