// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
