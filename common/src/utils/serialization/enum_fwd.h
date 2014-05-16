#ifndef QN_SERIALIZATION_ENUM_FWD_H
#define QN_SERIALIZATION_ENUM_FWD_H

/**
 * This macro enables serialization of the provided enumeration by casting it
 * to integer. Currently this can be used when serializing an enum to a binary
 * representation, or when storing it in a database.
 * 
 * Allowing such serialization for an enum requires a healthy dose of
 * self-discipline on the programmer's part --- once the enum is serialized,
 * its elements cannot be reordered, and it would actually be a good practice
 * to explicitly assign values to them.
 * 
 * This macro makes sure that the programmer doesn't forget about it.
 * 
 * \param ENUM
 */
#define QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ENUM)                              \
    inline void check_enum_binary(const ENUM *) {}

#endif // QN_SERIALIZATION_ENUM_FWD_H
