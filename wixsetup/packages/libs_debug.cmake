set(artifact.name.libs_debug ${artifact.name.libs_debug})
file(GLOB_RECURSE libs_debug_files
    "${bin_source_dir}/nx*.pdb"
    "${bin_source_dir}/ax*.pdb"
)

list(APPEND libs_debug_files "${bin_source_dir}/udt.pdb")