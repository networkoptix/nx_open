#ifndef QN_RESOURCE_PROPERTY_H
#define QN_RESOURCE_PROPERTY_H

#include <QtGlobal>

namespace QnResourceProperty {
    namespace {
        
        const char *id = "id";
        const char *typeId = "typeId";
        const char *uniqueId = "uniqueId";
        const char *name = "name";
        const char *searchString = "searchString";
        const char *status = "status";
        const char *flags = "flags";
        const char *url = "url";
        const char *tags = "tags";

        const char *properties[] = {id, typeId, uniqueId, name, searchString, status, flags, url, tags, NULL};

    } // anonymous namespace

    // TODO: #TEST

    /**
     * Tests that the QnResource actually exposes the properties defined in this file.
     */
    void test();

} // namespace QnResourceProperty

#endif // QN_RESOURCE_PROPERTY_H
