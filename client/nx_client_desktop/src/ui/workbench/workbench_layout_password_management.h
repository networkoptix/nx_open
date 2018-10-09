#pragma once

#include <core/resource/resource_fwd.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {
namespace layout {

/**
 * Returns true if layout is encrypted and password is unknown.
 * This function should be in QnLayoutResource, but it requires client-only LayoutEncryptionRole.
 */
bool needPassword(const QnLayoutResourcePtr& layout);

/**
* Asks password from user, checks it and then writes it to resource.
* Returns true if correct password was provided and set.
*/
bool askAndSetPassword(const QnLayoutResourcePtr& layout, QWidget* parent);

/** Asks password for already opened resource from user, and checks if it is valid */
bool confirmPassword(const QnLayoutResourcePtr& layout, QWidget* parent);

} // namespace layout
} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx