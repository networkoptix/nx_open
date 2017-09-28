def requestHost(customization, instance)
{
    def url = "http://ireg.hdw.mx/api/v1/cloudhostfinder/?group=$instance&vms_customization=$customization"
    println('Requesting cloudHost from ' + url)
    def result = new URL(url).text
    if (result)
        println("Resolved to ${result}")
    else
        throw "Cannot fetch cloud host for ${instance}/${customization}"
    return result
}

def host = properties.cloudHost?.trim()
def compatibleHosts = properties.compatibleHosts

if (!host)
{
    host = requestHost(properties.customization, properties.cloudGroup)

    def compatibleCustomizations = []
    if (properties.customization == "default")
        compatibleCustomizations = ["default_cn", "default_zh_CN"]
    else if (properties.customization == "digitalwatchdog")
        compatibleCustomizations = ["digitalwatchdog_global"]

    if (properties.cloudGroup != "dev") {
        compatibleHosts = []
        for (customization in compatibleCustomizations)
            compatibleHosts.add(requestHost(customization, properties.cloudGroup))
        if (compatibleHosts)
            compatibleHosts = compatibleHosts.join(";")
    }
}
else
{
    println("Using predefined cloudHost: " + host)
}

println("Writing cloud hosts to " + properties.targetPropertiesFile)
def file = new File(properties.targetPropertiesFile)
file.write("cloudHost=${host}\n")
file << "compatibleCloudHosts=${compatibleHosts}\n"
