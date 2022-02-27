// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class CustomizableItemDelegate;

/**
 * @return Custom accessor to get a string with resource version.
 */
QVariant resourceVersionAccessor(const QnResourcePtr& ptr, int role);

/**
 * @return Column-specific delegate, that applies color to resource version.
 */
CustomizableItemDelegate* makeVersionStatusDelegate(QnWorkbenchContext* context, QObject* parent);

} // namespace nx::vms::client::desktop
