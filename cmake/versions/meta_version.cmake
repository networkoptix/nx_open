## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(metaVersion R3)
if(customization.advanced.useMetaVersion)
    # Set meta-version to be shown in MetaVMS builds - in the Client, etc.
    set(usedMetaVersion ${metaVersion})
endif()
