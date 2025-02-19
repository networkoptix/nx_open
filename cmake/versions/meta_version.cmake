## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Version is reset on each minor release.
set(metaVersion R1)
if(customization.advanced.useMetaVersion)
    # Set meta-version to be shown in MetaVMS builds - in the Client, etc.
    set(usedMetaVersion ${metaVersion})
endif()
