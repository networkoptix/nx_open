#pragma once

#include <ui/graphics/instruments/instrument.h>
#include <ui/workbench/workbench_context_aware.h>

class QnGLCheckerInstrument: public Instrument, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Instrument;

public:
    QnGLCheckerInstrument(QObject* parent = nullptr);
    virtual ~QnGLCheckerInstrument() override;

protected:
    virtual bool registeredNotify(QWidget *viewport) override;
    virtual void unregisteredNotify(QWidget *viewport) override;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};
