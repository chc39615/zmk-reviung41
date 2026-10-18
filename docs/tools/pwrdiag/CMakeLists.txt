if(CONFIG_PWRDIAG)
  zephyr_library()
  zephyr_library_sources(src/pwrdiag.c)
endif()
