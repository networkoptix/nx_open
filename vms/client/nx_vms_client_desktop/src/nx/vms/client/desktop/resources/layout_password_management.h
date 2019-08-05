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

/** Returns true if layout is encrypted. */
bool isEncrypted(const QnLayoutResourcePtr& layout);

/** Retrieve actual entered password. */
QString password(const QnLayoutResourcePtr& layout);

/**
* Asks password from user, checks it and then writes it to resource.
* Returns true if correct password was provided and set.
*/
QString askPassword(const QnLayoutResourcePtr& layout, QWidget* parent);

/** Asks password for already opened resource from user, and checks if it is valid */
bool confirmPassword(const QnLayoutResourcePtr& layout, QWidget* parent);

} // namespace nx::vms::client::desktop::layout
