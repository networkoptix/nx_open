# Markdown Dialect  {#markdown}

Unfortunately, Markdown exist in many flavors. VMS documentation should support Markdown features of at least Doxygen and possibly Upsource.

## Tutorials and referencies

Markdown tutorial: https://www.markdowntutorial.com/

Markdown basics on GitHub: https://help.github.com/articles/basic-writing-and-formatting-syntax/

Complete Markdown reference for Doxygen: http://doxygen.nl/manual/markdown.html 

## Basic recommended Syntax

Here comes some basic recommended Doxygen Markdown syntax. This is mostly based on Doxygen manual, but some variations are omitted in favor of others.

### Standard Markdown


#### Emphasis

To emphasize a text fragment you start and end the fragment with a star. Using two stars  will produce strong emphasis.

Examples:

~~~
*single asterisks*

**double asterisks**
~~~
 
 To indicate a span of code, you should wrap it in backticks (`). Unlike code blocks, code spans appear inline in a paragraph. An example:

~~~ 
Use the `printf()` function.
~~~

 See also [Doxygen exceptions](#exceptions).

To show a literal backtick inside a code span use double backticks, i.e.

~~~
To assign the output of command `ls` to `var` use ``var=`ls```.
~~~

#### Paragraphs

To make a paragraph you just separate consecutive lines of text by one or more blank lines.

#### Headers

You can use #'s at the start of a line to make a header. The number of #'s at the start of the line determines the level (up to 6 levels are supported). You can end a header by any number of #'s.

Here is an example:

~~~
# This is a level 1 header
### This is level 3 header
~~~

#### Block quotes

Block quotes can be created by starting each line with one or more >'s, similar to what is used in text-only emails.

~~~
> This is a block quote
> spanning multiple lines
~~~

Lists and code blocks (see below) can appear inside a quote block. Quote blocks can also be nested.

#### Lists

Simple bullet lists can be made by starting a line with -, +, or *.

~~~
- Item 1

  More text for this item.

- Item 2
  + nested list item.
  + another nested item.
- Item 3
~~~

List items can span multiple paragraphs (if each paragraph starts with the proper indentation) and lists can be nested. You can also make a numbered list like so

~~~
1. First item.
2. Second item.
~~~

#### Code Blocks

Preformatted verbatim blocks can be created by indenting each line in a block of text by at least 4 extra spaces

~~~
This a normal paragraph

    This is a code block
~~~    

We continue with a normal paragraph again.

Doxygen will remove the mandatory indentation from the code block. Note that you cannot start a code block in the middle of a paragraph (i.e. the line preceding the code block must be empty).

#### Horizontal Rulers

A horizontal ruler will be produced for lines containing at least three hyphens.

~~~
______
~~~

#### Links

Doxygen supports both styles of make links defined by Markdown: inline and reference.

For both styles the link definition starts with the link text delimited by [square brackets].


##### Inline Links

For an inline link the link text is followed by a URL and an optional link title which together are enclosed in a set of regular parenthesis. The link title itself is surrounded by quotes.

~~~
[The link text](http://example.net/)
[The link text](http://example.net/ "Link title")
[The link text](/relative/path/to/index.html "Link title") 
[The link text](somefile.html) 
~~~

In addition doxygen provides a similar way to link a documented entity:

~~~
[The link text](@ref MyClass) 
~~~

##### Reference Links

Instead of putting the URL inline, you can also define the link separately and then refer to it from within the text.

The link definition looks as follows:
~~~
[link name]: http://www.example.com "Optional title"
~~~

Instead of double quotes also single quotes or parenthesis can be used for the title part.

Once defined, the link looks as follows

~~~
[link text][link name]
~~~

If the link text and name are the same, also

~~~
[link name][]
~~~

or even

~~~
[link name]
~~~

can be used to refer to the link. Note that the link name matching is not case sensitive.

##### Anchor Links

To create anchor on a page, use \@anchor syntax:
~~~
# Topic name @anchor topicname
~~~
You may then link to the anchor as simple as:
~~~
[link text](#topicname)
~~~

Note: /{#topicname/} declaration of anchor links may work incorrectly in Doxygen.


##### Links to markdown files

Links to Markdown files are created this way:
~~~
[Link text](topicname.html)
~~~

Please note that you should use topicname (the name that goes in the header in curly braces `{#topicname}`) with .html extension.

Another option is:

~~~
[Link text](@ref topicname)
~~~

#### Images

Markdown syntax for images is similar to that for links. The only difference is an additional ! before the link text.

~~~
![Caption text](/path/to/img.jpg)
![Caption text](/path/to/img.jpg "Image title")
![Caption text][img def]
![img def]

[img def]: /path/to/img.jpg "Optional Title"
~~~

#### Automatic Linking

To create a link to an URL or e-mail address just write id down without any extra markup.
Please do not use automatic linking using angular brackets.

### Markdown Extensions

#### Tables

Of the features defined by "Markdown Extra" is support for simple tables:

A table consists of a header line, a separator line, and at least one row line. Table columns are separated by the pipe (|) character.


~~~
First Header  | Second Header
------------- | -------------
Content Cell  | Content Cell 
Content Cell  | Content Cell 
~~~
will produce the following table:

First Header  | Second Header
------------- | -------------
Content Cell  | Content Cell 
Content Cell  | Content Cell 

Column alignment can be controlled via one or two colons at the header separator line:

~~~
| Right | Center | Left  |
| ----: | :----: | :---- |
| 10    | 10     | 10    |
| 1000  | 1000   | 1000  |
~~~

which will look as follows:

| Right | Center | Left  |
| ----: | :----: | :---- |
| 10    | 10     | 10    |
| 1000  | 1000   | 1000  |

#### Fenced code blocks

A fenced code block does not require indentation, and is defined by a pair of "fence lines". Such a line consists of 3 or more tilde (~) characters on a line. The end of the block should have the same number of tildes. Here is an example:

This is a paragraph introducing:

```
~~~
a one-line code block
~~~
```

#### Header Id Attributes

Standard Markdown has no support for labeling headers, which is a problem if you want to link to a section.

PHP Markdown Extra allows you to label a header by adding the following to the header


~~~
## Header 2 ##          {#labelid2}
~~~

To link to a section in the same comment block you can use

~~~
[Link text](#labelid)
~~~

to link to a section in general, doxygen allows you to use @ref

~~~
[Link text](@ref labelid)
~~~

Note this only works for the headers of level 1 to 4.

### Doxygen specifics @anchor exceptions

#### Treatment of HTML blocks

Markdown is quite strict in the way it processes block-level HTML:

block-level HTML elements — e.g. \<div\>, \<table\>, \<pre\>, \<p\>, etc. — must be separated from surrounding content by blank lines, and the start and end tags of the block should not be indented with tabs or spaces.

Doxygen does not have this requirement, and will also process Markdown formatting inside such HTML blocks. The only exception is \<pre\> blocks, which are passed untouched (handy for ASCII art).

Doxygen will not process Markdown formatting inside verbatim or code blocks, and in other sections that need to be processed without changes (for instance formulas or inline dot graphs).

#### Emphasis limits

Unlike standard Markdown, doxygen will not touch internal underscores or stars, so the following will appear as-is:

a_nice_identifier

Furthermore, a * or _ only starts an emphasis if

    it is followed by an alphanumerical character, and
    it is preceded by a space, newline, or one the following characters <{([,:;

An emphasis ends if

    it is not followed by an alphanumerical character, and
    it is not preceded by a space, newline, or one the following characters ({[<=+-\@

Lastly, the span of the emphasis is limited to a single paragraph.
