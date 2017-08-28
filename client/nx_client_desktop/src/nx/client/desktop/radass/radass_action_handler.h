#pragma once

#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {

class RadassActionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    RadassActionHandler(QObject* parent = nullptr);
    virtual ~RadassActionHandler() override;

private:
    void at_radassAction_triggered();

};

} // namespace desktop
} // namespace client
} // namespace nx