def requestHost(customization, instance)
{
    def url = "http://ireg.hdw.mx/api/v1/cloudhostfinder/?group=$instance&vms_customization=$customization"
    println('Requesting cloudHost from ' + url)
    return new URL(url).text
}

def host = properties.cloudHost?.trim()

if (!host)
{
    host = requestHost(properties.customization, properties.cloudGroup)
    println("cloudHost resolved to " + host)
    for (project in session.getProjects())
        project.properties.cloudHost = host
}
else
{
    println("Using predefined cloudHost: " + properties.cloudHost)
}
