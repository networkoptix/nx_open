// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <analytics/common/object_metadata.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>

namespace nx::vms::client::desktop {

class NameValueTable: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    NameValueTable(QWidget* parent = nullptr);
    virtual ~NameValueTable() override;

    core::analytics::AttributeList content() const;

    // Warning! Calling setContent during update of a backing store of some OpenGL-backed window
    // must be avoided as OpenGL context switching at that moment can cause bugs at some systems.
    void setContent(const core::analytics::AttributeList& value);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    virtual void paintEvent(QPaintEvent* event) override;

    virtual bool event(QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
