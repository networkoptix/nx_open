set(artifact.name.server_debug ${artifact.name.server_debug})
file(GLOB_RECURSE server_debug_files
    "${bin_source_dir}/plugins/*.pdb"
)

list(APPEND server_debug_files "${bin_source_dir}/mediaserver.pdb")