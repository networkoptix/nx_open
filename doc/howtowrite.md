# How to write documentation  {#howto}

## Write a new topic

Normally topics go in the *readme.md* files that are located in the directory with code that they describe.

- Please start file with __"# Topic Name \{#topicname\}"__. This will make __"Topic Name"__ to appear in contents and also enables linking this page with __"topicname"__ tag.
- Add __"@subpage topicname"__ to */doc/Topics.md* . This will place the topic into the "VMS Code Topics" section. 
- Please use unique names for images - doxygen uses quite non-obvious rules for finding images, so names should be always different. 

## Add an image to a topic

Create a *doc* subfolder at the same level as Markdown files and put the image inside it. Embed it into topic using __\!\[image\](doc/imagename.png)__ syntax.


