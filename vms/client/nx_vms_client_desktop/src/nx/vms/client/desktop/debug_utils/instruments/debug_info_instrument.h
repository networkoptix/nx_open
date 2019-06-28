#pragma once

#include <QtCore/QVector>
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

    std::vector<qint64> getFrameTimePoints();

signals:
    void debugInfoChanged(const QString& richText);

protected:
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

    virtual bool paintEvent(QWidget* viewport, QPaintEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // nx::vms::client::desktop
