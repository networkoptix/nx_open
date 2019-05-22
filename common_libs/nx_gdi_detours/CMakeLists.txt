nx_add_target(nx_gdi_detours LIBRARY NO_MOC)
target_link_libraries(nx_gdi_detours PUBLIC
	detours
	stackwalker
)
