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
}
else
{
    println("Using predefined cloudHost: " + host)
}

println("Writing cloudHost: " + host + " to file " + properties.targetPropertiesFile)
def file = new File(properties.targetPropertiesFile)
file.write("cloudHost=" + host)
