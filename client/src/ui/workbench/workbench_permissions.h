#ifndef QN_WORKBENCH_PERMISSIONS_H
#define QN_WORKBENCH_PERMISSIONS_H

#include <QtCore/QtGlobal>

namespace Qn {
    enum Permission {
        /* Generic permissions. */

        /** Generic read access. Having this access right doesn't necessary mean that all information is readable. */
        ReadPermission              = 0x00000001,   

        /** Generic write access. Having this access right doesn't necessary mean that all information is writable. */ 
        WritePermission             = 0x00000002,   

        /** Generic save access. Entity can be saved to appserver. */
        SavePermission              = 0x00000004,   

        /** Generic delete permission. */
        RemovePermission            = 0x00000008,

        /** Generic read-write-save permission. */
        ReadWriteSavePermission     = ReadPermission | WritePermission | SavePermission,



        /* User-specific permissions. */

        /** Permission to view associated password. Note that not all passwords are viewable even with this flag. */
        ReadPasswordPermission      = 0x00000010,

        /** Permission to edit associated password. */
        WritePasswordPermission     = 0x00000100,

        /** Permission to edit access rights. */
        WriteAccessRightsPermission = 0x00000200,

        /** Permission to create layouts for the user. */
        CreateLayoutPermission      = 0x00002000,



        /* Global permissions. */

        /** Permission to create users. */
        CreateUserPermission        = 0x00001000,

        AllPermissions              = 0xFFFFFFFF
    };

    Q_DECLARE_FLAGS(Permissions, Permission);

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::Permissions);

#endif // QN_WORKBENCH_PERMISSIONS_H
