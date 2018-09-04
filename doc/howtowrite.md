# How to write documentation  {#howto}

## Write a new topic

Normally topics go in the *readme.md* files that are located in the directory with code that they describe. Some general topics are placed in /doc directory of the project tree.

- Please start file with "# Topic Name {#topicname}". This will make "Topic Name" to appear in contents and also enables linking this page with __topicname__ tag.
- Add "@subpage topicname" to /doc/Topics.md . This will place the topic into the "VMS Code Topics" section. 
- Please use unique names for images - doxygen uses quite non-obvious rules for finding images, so names should be always different. 

## Add an image to a topic

Create a *doc* subfolder at the same level as Markdown files and put the image inside it. Embed it into topic using \!\[image\](doc/imagename.png) syntax.

## [Markdown dialect](markdown.html)

Unfortunately, Markdown has many dialects. We suggest using only basic Markdown syntax that is common for both Doxygen and Upsource.
