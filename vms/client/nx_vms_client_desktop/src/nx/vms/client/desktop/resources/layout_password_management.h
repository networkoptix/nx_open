#pragma once

#include <core/resource/resource_fwd.h>

/**
 * This module provides some functions fo working with encrypted layouts in client.
 * Some of them could be in QnLayoutResource, but it knows nothing about data roles
 * which contain password information.
 */

class QWidget;
class QString;

namespace nx::vms::client::desktop::layout {

/** Mark layout as encrypted. */
void markAsEncrypted(const QnLayoutResourcePtr& layout, bool value = true);

/** Returns true if layout is encrypted. */
bool isEncrypted(const QnLayoutResourcePtr& layout);

/** Retreive actual entered password. */
QString password(const QnLayoutResourcePtr& layout);

/** Returns true if layout is encrypted and password is unknown. */
bool requiresPassword(const QnLayoutResourcePtr& layout);

/** Set layout password for reading, no checking. Propagates to layout children. */
void usePasswordToOpen(const QnLayoutResourcePtr& layout, const QString& password);

/** Reset layout password. Propagates to layout children. */
void forgetPassword(const QnLayoutResourcePtr& layout);

/**
* Asks password from user, checks it and then writes it to resource.
* Returns true if correct password was provided and set.
*/
bool askAndSetPassword(const QnLayoutResourcePtr& layout, QWidget* parent);

/** Asks password for already opened resource from user, and checks if it is valid */
bool confirmPassword(const QnLayoutResourcePtr& layout, QWidget* parent);

} // namespace nx::vms::client::desktop::layout
