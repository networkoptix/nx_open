nx_add_target(nx_gdi_tracer LIBRARY NO_MOC)
target_link_libraries(nx_gdi_tracer PUBLIC
	detours
	stackwalker
)
