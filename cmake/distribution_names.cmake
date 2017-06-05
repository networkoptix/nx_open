function(set_distribution_names)
    set(distribution_name_prefix ${installer.name})
    if(beta)
        set(beta_suffix "-beta")
        set(distribution_name_suffix
            "${releaseVersion.full}-${platform}${beta_suffix}-${cloudGroup}")
    else()
        set(beta_suffix)
        set(distribution_name_suffix)
    endif()

    set(client_distribution_name
        "${distribution_name_prefix}-client-${distribution_name_suffix}"
        PARENT_SCOPE)
endfunction()

set_distribution_names()
