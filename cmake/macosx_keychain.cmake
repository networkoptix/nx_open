cmake_dependent_option(useLoginKeychain "Use Login keychain and do not create a temporary one"
    ON "developerBuild"
    OFF
)
set(codeSigningKeychainName "nx_build" CACHE INTERNAL "")
set(codeSigningKeychainPassword "qweasd123" CACHE INTERNAL "")

if(useLoginKeychain)
    set(codeSigningKeychainName)
endif()

if(NOT useLoginKeychain AND NOT (mac.skip.sign AND ios.skip.sign))
    nx_find_first_matching_file(certificate "${certificates_path}/macosx/*.p12")
    if(NOT certificate)
        message(FATAL_ERROR "Cannot find any certificates in ${certificates_path}/macosx")
    endif()

    add_custom_target(prepare_mac_keychain
        ${CMAKE_SOURCE_DIR}/build_utils/macos/prepare_build_keychain.sh
            --keychain ${codeSigningKeychainName}
            --keychain-password ${codeSigningKeychainPassword}
            --certificate ${certificate}
            --certificate-password ${mac_certificate_file_password}
    )
endif()

function(prepare_mac_keychain target)
    if(TARGET prepare_mac_keychain)
        add_dependencies(${target} prepare_mac_keychain)
    endif()
endfunction()
