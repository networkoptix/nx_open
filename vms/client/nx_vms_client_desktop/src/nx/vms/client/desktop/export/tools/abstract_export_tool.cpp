#include "abstract_export_tool.h"

namespace nx::vms::client::desktop {

AbstractExportTool::AbstractExportTool(QObject* parent):
    base_type(parent)
{
}

AbstractExportTool::~AbstractExportTool()
{
}

} // namespace nx::vms::client::desktop
