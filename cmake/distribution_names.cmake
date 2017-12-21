function(set_distribution_names)
    set(prefix ${installer.name})
    if(beta)
        set(beta_suffix "-beta")
        set(suffix "${platform}${beta_suffix}-${cloudGroup}")
    else()
        set(beta_suffix)
        set(suffix "${platform}-${cloudGroup}")
    endif()

    set(client_distribution_name "${prefix}-client-${releaseVersion.full}-${suffix}"
        PARENT_SCOPE)
    set(mobile_client_distribution_name "${prefix}-client-${mobileClientVersion.full}-${suffix}"
        PARENT_SCOPE)
endfunction()

set_distribution_names()
