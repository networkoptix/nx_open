# This files defines macro controlling compile-time presence of certain functionality


# DISABLE_ALL_VENDORS introduced to simplify enabling specified vendors. E.g. to enable onvif only define DISABLE_ALL_VENDORS and ENABLE_ONVIF macros
#   note: at the moment only onvif functionality can be enabled/disabled

!contains( DEFINES, DISABLE_ALL_VENDORS ) {
  !contains( DEFINES, DISABLE_ONVIF ) {
    DEFINES += ENABLE_ONVIF
  }
}

IS_VMAX_ENABLED=${vmax}
contains( IS_VMAX_ENABLED, true ) {
  DEFINES += ENABLE_VMAX
}
