#pragma once

namespace nx {
namespace utils {

/** Class that is a base for encryptable entities. Methods can be overriden partially. */

class Encryptable
{
    ~Encryptable() = default;

    /** Returns true if the entity is actually encrypted. */
    virtual bool isEncrypted() const { return false; }
    /** Returns true if the entity is encrypted and no valid password is provided. */
    virtual bool requiresPassword() const { return false; }
    /** Attempts to set a password for opening existing entity. */
    virtual bool usePasswordToRead(const QString& password) { return true; }
    /** Sets a password for writing new entity. */
    virtual void setPasswordToWrite(const QString& password) {}
    /** Drops password. */
    virtual void dropPassword() {}
};

} // namespace utils
} // namespace nx
