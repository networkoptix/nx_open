if(customization.id)
    # Customization may forecefully disable the code signing.
    message(FATAL_ERROR "Variable must be defined before customization is loaded")
endif()

# Enable code signing for all publication types except local (developer one).
set(codeSigning ON)
if(publicationType STREQUAL "local")
    set(codeSigning OFF)
endif()
message(STATUS "Code signing is ${codeSigning} for the ${publicationType} publication type")
