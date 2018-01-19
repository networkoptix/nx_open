cmake_dependent_option(useLoginKeychain "Use Login keychain and do not create a temporary one"
    ON "developerBuild"
    OFF
)
set(codeSigningKeychainName "nx_build" CACHE INTERNAL "")
set(codeSigningKeychainPassword "qweasd123" CACHE INTERNAL "")

if(useLoginKeychain)
    set(codeSigningKeychainName)
endif()

