def process_file(cmake_file_name, properties_file_name)
{
    def cmake_file_content = new File(cmake_file_name).text

    def properties_file = new File(properties_file_name)
    properties_file.write("[basic]\n")

    // Replace construction like
    // set(variable
    //     value1
    //     "value2"
    //     "value_3_(with_braces)"
    // )
    // with
    // variable=value1 value2 value_3_(with_braces)

    def matcher = cmake_file_content =~
        '(?s)set\\(([^\\s]+)\\s*((?:".*?"|.*?){1,})\\s*\\)'

    matcher.each { matched, variable, expression ->
        expression = expression
            .replaceAll('\\s+', ' ')
            .replaceAll('(?<!\\\\)"', '')
            .replaceAll('\\\\"', '"')
        properties_file << "${variable}=${expression}\n"
    }
}

def root = project.properties["root.dir"]

process_file("${root}/customization/${project.properties.customization}/customization.cmake",
    "${root}/.customization.properties")
