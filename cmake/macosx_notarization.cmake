include_guard(GLOBAL)

if(NOT MACOSX OR NOT codeSigning)
    return()
endif()

# Enable notarization for all publication types intended for end users. Do not notarize local
# developer builds as well as private QA builds.
set(notarization ON)
set(_disableNotarizationPublicationTypes "local" "private")
if(publicationType IN_LIST _disableNotarizationPublicationTypes)
    set(notarization OFF)
endif()
unset(_disableNotarizationPublicationTypes)
message(STATUS "Notarization is ${notarization} for the ${publicationType} publication type")
