# This files defines macro controlling compile-time presence of certain functionality


# DISABLE_ALL_VENDORS introduced to simplify enabling specified vendors. E.g. to enable onvif only define DISABLE_ALL_VENDORS and ENABLE_ONVIF macros
#   note: at the moment only onvif functionality can be enabled/disabled

!contains( DEFINES, DISABLE_ALL_VENDORS ) {
  !contains( DEFINES, DISABLE_ONVIF ) {
    DEFINES += ENABLE_ONVIF
  }
  !contains( DEFINES, DISABLE_AXIS ) {
    DEFINES += ENABLE_AXIS
  }
  !contains( DEFINES, DISABLE_ACTI ) {
    DEFINES += ENABLE_ACTI
  }
  !contains( DEFINES, DISABLE_ARECONT ) {
    DEFINES += ENABLE_ARECONT
  }
  !contains( DEFINES, DISABLE_DLINK ) {
    DEFINES += ENABLE_DLINK
  }
  !contains( DEFINES, DISABLE_DROID ) {
    DEFINES += ENABLE_DROID
  }
  !contains( DEFINES, DISABLE_TEST_CAMERA ) {
    DEFINES += ENABLE_TEST_CAMERA
  }
  !contains( DEFINES, DISABLE_STARDOT) {
    DEFINES += ENABLE_STARDOT
  }
  !contains( DEFINES, DISABLE_IQE) {
    DEFINES += ENABLE_IQE
  }
  !contains( DEFINES, DISABLE_ISD) {
    DEFINES += ENABLE_ISD
  }
  !contains( DEFINES, DISABLE_PULSE_CAMERA) {
    DEFINES += ENABLE_PULSE_CAMERA
  }
}

!contains( DEFINES, DISABLE_MDNS) {
  DEFINES += ENABLE_MDNS
}

!contains( DEFINES, DISABLE_COLDSTORE) {
  DEFINES += ENABLE_COLDSTORE
}

!contains( DEFINES, DISABLE_ARCHIVE ) {
  DEFINES += ENABLE_ARCHIVE
}

!contains( DEFINES, DISABLE_THIRD_PARTY ) {
  DEFINES += ENABLE_THIRD_PARTY
}

!contains( DEFINES, DISABLE_SOFTWARE_MOTION_DETECTION ) {
  DEFINES += ENABLE_SOFTWARE_MOTION_DETECTION
}

!contains( DEFINES, DISABLE_DESKTOP_CAMERA ) {
  DEFINES += ENABLE_DESKTOP_CAMERA
}

!contains( DEFINES, DISABLE_DATA_PROVIDERS) {
  DEFINES += ENABLE_DATA_PROVIDERS
}

!contains( DEFINES, DISABLE_SSL) {
  DEFINES += ENABLE_SSL
}

!contains( DEFINES, DISABLE_BOOKMARKS) {
  DEFINES += QN_ENABLE_BOOKMARKS
}

contains(NAME, mediaserver) || contains(NAME, common) {
  IS_VMAX_ENABLED=${vmax}
  contains( IS_VMAX_ENABLED, true ) {
    DEFINES += ENABLE_VMAX
  }
}

contains(NAME, client) {
  IS_DYNAMIC_CUSTOMIZATION_ENABLED=${dynamic.customization}
  contains( IS_DYNAMIC_CUSTOMIZATION_ENABLED, true ) {
    DEFINES += ENABLE_DYNAMIC_CUSTOMIZATION
  }
}