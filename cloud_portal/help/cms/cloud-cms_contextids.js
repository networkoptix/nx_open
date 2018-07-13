var hmContextIds = new Array();
function hmGetContextId(query) {
    var urlParams;
    var match,
        pl = /\+/g,
        search = /([^&=]+)=?([^&]*)/g,
        decode = function (s) { return decodeURIComponent(s.replace(pl, " ")); },
    params = {};
    while (match = search.exec(query))
       params[decode(match[1])] = decode(match[2]);
    if (params["contextid"]) return decodeURIComponent(hmContextIds[params["contextid"]]);
    else return "";
}

hmContextIds["240"]="cms_overview.htm";
hmContextIds["249"]="logging_in_to_cms.htm";
hmContextIds["251"]="portal_management.htm";
hmContextIds["241"]="content_management.htm";
hmContextIds["242"]="editing_pages.htm";
hmContextIds["256"]="editing_pages.htm#htmlfields";
hmContextIds["255"]="editing_pages.htm#selectingalanguage";
hmContextIds["248"]="branding_elements.htm";
hmContextIds["258"]="branding_elements.htm#productname";
hmContextIds["260"]="branding_elements.htm#supportlink";
hmContextIds["259"]="branding_elements.htm#privacypolicylink";
hmContextIds["244"]="cloud_maintenance_503_page.htm";
hmContextIds["247"]="landing_page.htm";
hmContextIds["246"]="license_agreement_page.htm";
hmContextIds["243"]="privacy_policy_page.htm";
hmContextIds["245"]="support_page.htm";
hmContextIds["250"]="reviewing_revisions.htm";
hmContextIds["257"]="reviewing_revisions.htm#forceupdate";
hmContextIds["252"]="cloud_users.htm";
hmContextIds["253"]="accounts.htm";
