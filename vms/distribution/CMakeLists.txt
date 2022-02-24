## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_configure_file(${customization_dir}/license.html
    ${distribution_output_dir}/eula-${customization.eulaVersion}.html)
nx_copy(${customization_dir}/description.json DESTINATION ${distribution_output_dir})

if(WINDOWS)
    add_subdirectory(wix)
elseif(platform STREQUAL "linux")
    add_subdirectory(deb)
elseif(MACOSX)
    add_subdirectory(dmg)
endif()

if(withUnitTestsArchive)
    add_subdirectory(unit_tests)
endif()
