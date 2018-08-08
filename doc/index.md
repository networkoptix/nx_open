# Welcome to the VMS Code Doc {#mainpage}

VMS Documentation is generated with DoxyGen software http://www.doxygen.nl/index.html .

## Documentation sources


Documentation is generated from documentation embedded in source files as well as stand-alone topics.

### Embedded documentation

Embedded docs are contained inside source code should follow javadoc style - see our Code Style Guide - https://networkoptix.atlassian.net/wiki/spaces/SD/pages/44531791/+Coding+Style#id-%D0%A1++CodingStyle-Doxygen .
DoxyGen does a great job finding most of language entities, extracting relevant comments and making interlinked HTML documentation.
Embedded documentation is focused mostly on the low-level code information.

### Stand-alone topics

Higher-level documentation resides in *readme.md* files inside code tree. Those files are in Markdown format. Markdown is a lightweight extension to the normal text files, and Doxygen supports it directly
http://doxygen.nl/manual/markdown.html . Markdown files are also natively supported in Upsource viewer - 
https://www.jetbrains.com/help/upsource/markdown-syntax.html . It is strongly recommended to use only Markdown features supported by both Doxygen and Upsource.

Normally topics go in the *readme.md* files that are located in the directory with code that they describe. An example of such readme can be found in folder \client\nx_client_desktop\src\ui\graphics\items\controls\ . It is allowed to have more than one doc file in a direcory if needed, so that the main one should be named *readme.md* and others should have also *.md* extension.

Images should be located in *doc* subfolder created at the same level as *readme.md* file itself.

@subpage howto

## Generating Docs

To generate VMS documentation, enter */doc* directory and execute *generate.cmd* for Windows or *generate.cmd* for macOS or Linux.


