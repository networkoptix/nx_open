// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

export const kSchemes = ["ldap://", "ldaps://"]

export function splitUrl(uri)
{
    const lowerCaseUrl = uri.toLowerCase()

    for (const scheme of kSchemes)
    {
        if (lowerCaseUrl.startsWith(scheme))
            return {"hostAndPort": uri.substring(scheme.length), "scheme": scheme}
    }

    return  {"hostAndPort": uri, "scheme": "ldap://"}
}
