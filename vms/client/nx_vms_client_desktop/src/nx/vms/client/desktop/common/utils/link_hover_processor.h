// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

class QLabel;

namespace nx::vms::client::desktop {

/*
 * Class to dynamically change hovered/unhovered label links color and cursor.
 *  Color is changed to style::linkColor(m_label, isHovered).
 *  It uses Link and LinkVisited palette entries.
 */
class LinkHoverProcessor: public QObject
{
    Q_OBJECT

public:
    explicit LinkHoverProcessor(QLabel* parent);
    virtual ~LinkHoverProcessor() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
