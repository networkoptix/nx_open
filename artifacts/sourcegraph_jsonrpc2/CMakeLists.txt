include(go/go)

if(withClouds)
    nx_go_add_src_only_target(nx_go_sourcegraph_jsonrpc2 "")
endif()
