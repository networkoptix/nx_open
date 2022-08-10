## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(${open_source_root}/cmake/distribution/flavors.cmake)

nx_configure_file(${customization_dir}/license.html
    ${distribution_output_dir}/eula-${customization.eulaVersion}.html)
nx_copy(${customization_dir}/description.json DESTINATION ${distribution_output_dir})
if(WINDOWS)
    add_subdirectory(wix)
elseif(platform STREQUAL "linux")
    nx_add_subdirectories_for_flavors(deb)
elseif(MACOSX)
    add_subdirectory(dmg)
endif()

if(withUnitTestsArchive)
    add_subdirectory(unit_tests)
endif()
