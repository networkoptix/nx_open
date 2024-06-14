// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::api { enum class WebPageSubtype; }

class QnWorkbenchWebPageHandler:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnWorkbenchWebPageHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchWebPageHandler();

private:
    void addNewWebPage(nx::vms::api::WebPageSubtype subtype);
    void editWebPage();
    void openInDedicatedWindow(const QnWebPageResourcePtr& webPage);
};
