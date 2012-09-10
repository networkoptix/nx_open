#ifndef QN_MODULE_RESOURCES_H
#define QN_MODULE_RESOURCES_H

#include <QtCore/QtGlobal>

/**
 * Initializes all resources associated with the given module.
 * 
 * See <tt>cpp/shared-resources/gen-resources.py</tt> for details on how
 * these resources are generated.
 * 
 * \param MODULE_NAME                   Name of the module to initialize resources for.
 */
#define QN_INIT_MODULE_RESOURCES(MODULE_NAME)                                   \
    {                                                                           \
        Q_INIT_RESOURCE(MODULE_NAME);                                           \
        Q_INIT_RESOURCE(MODULE_NAME ## _common);                                \
        Q_INIT_RESOURCE(MODULE_NAME ## _custom);                                \
        Q_INIT_RESOURCE(MODULE_NAME ## _generated);                             \
        Q_INIT_RESOURCE(MODULE_NAME ## _translations);                          \
    }

#endif // QN_MODULE_RESOURCES_H
