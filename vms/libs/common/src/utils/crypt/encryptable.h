#pragma once

#include <QtCore/QString>

namespace nx::utils {

/**
 * Class that is a concept base for encryptable entities. Default realization is private.
 * Methods can be overriden partially.
 */

class Encryptable
{
public:
    virtual ~Encryptable() = default;

protected: //< These functions do nothing and should be overwritten as public, at least partually.
    /** Returns true if the entity is actually encrypted. */
    virtual bool isEncrypted() const { return false; }
    /** Returns true if the entity is encrypted and no valid password is provided. */
    virtual bool requiresPassword() const { return isEncrypted() && password().isEmpty(); }
    /** Attempts to set a password for opening existing entity. */
    virtual bool usePasswordToRead(const QString& /*password*/) { return true; }
    /** Sets a password for writing new entity. */
    virtual void setPasswordToWrite(const QString& /*password*/) {}
    /** Drops password. */
    virtual void forgetPassword() { usePasswordToRead({}); }
    /** Returns password */
    virtual QString password() const { return {}; }
};

} // namespace nx::utils
