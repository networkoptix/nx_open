def getProjectType(String artifactId)
{
    for (project in session.getProjects())
    {
        if (project.artifactId == artifactId)
            return project.properties.projectType
    }
    return "generic"
}

def proFilePath = project.properties["root.dir"] +
    "/vms-" +
    project.properties["build.suffix"] +
    ".pro"

def file = new File(proFilePath)

file.write("TEMPLATE = subdirs\n\n")

for (project in session.getProjects())
{
    if (project.properties.projectType != "cpp")
        continue

    file << "SUBDIRS += ${project.artifactId}\n"
    file << "${project.artifactId}.file = ${project.build.outputDirectory}/${project.artifactId}.pro\n"

    def deps = []
    for (dependency in project.dependencies)
    {
        if (getProjectType(dependency.artifactId) == "cpp")
            deps.add(dependency.artifactId)
    }
    if (deps.size() > 0)
        file << "${project.artifactId}.depends = " + deps.join(" ") + "\n"

    file << "\n"
}
