if (NOT PROJECT_DISPLAY_LONGNAME)
  set (PROJECT_DISPLAY_LONGNAME "${PROJECT_LONGNAME}")
endif()
set(project.artifactId "${PROJECT_SHORTNAME}")

#This properties are introduced for compatibility with Maven build.
set(product.display.title "${PROJECT_DISPLAY_LONGNAME}")
set(product.title "${PROJECT_LONGNAME}")
set(customization.dir "${CMAKE_SOURCE_DIR}/customization/${customization}")