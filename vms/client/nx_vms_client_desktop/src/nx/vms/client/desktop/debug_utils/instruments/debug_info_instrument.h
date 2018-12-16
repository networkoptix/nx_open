#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QWidget>

#include <ui/graphics/instruments/instrument.h>

namespace nx::vms::client::desktop {

class DebugInfoInstrument: public Instrument
{
    Q_OBJECT

public:
    explicit DebugInfoInstrument(QObject* parent = nullptr);
    virtual ~DebugInfoInstrument() override;

signals:
    void debugInfoChanged(const QString& text);

protected:
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

    virtual bool paintEvent(QWidget* viewport, QPaintEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // nx::vms::client::desktop
