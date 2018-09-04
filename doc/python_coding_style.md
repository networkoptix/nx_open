# Python Coding Style

Since there exist widely accepted [PEP 8][] (general style), [PEP 257][] (docstrings), [PEP 484][]
(type hinting) and this coding style is built upon them. All PEP 8, PEP 257 and PEP 484
restrictions still hold.

In some cases new restrictions are imposed:
- to comply with [C++ Coding Style][],
- to follow the style of the existing code.

To remove burden of manual reformatting, this coding style is supported by most tools (pylint,
PyCharm inspections) with limited number of non-default settings.

Below are only:
- additional restrictions and refinements to PEP 8,
- differences from C++ Coding Style.

@anchor maximum-line-length

## Maximum Line Length

99 characters.

## Multiline Calls and Statements

```python
foo = long_function_name(
    var_one, var_two,
    var_three, var_four)
```
```python
def long_function_name(
        var_one, var_two, var_three,
        var_four):
    print(var_one)
```

Break the line before the first argument if call or signature is multilined.

## Closing Parenthesis/Bracket/Brace

When closing parenthesis/bracket/brace is on the separate line, only the following style is used:
```python
my_list = [
    1, 2, 3,
    4, 5, 6,
]
```
```python
result = some_function_that_takes_arguments(
    'a', 'b', 'c',
    'd', 'e', 'f',
)
```

Closing brace has the same indent as first line of expression/statement.

If closing parenthesis/bracket/brace is on the separate line, last element should be followed by
comma. Otherwise, it should not.

## Inline Comments

[PEP 8 section Inline Comments][]:
> An inline comment is a comment on the same line as a statement. Inline comments should be
separated by at least two spaces from the statement. They should start with a `#` and a single
space.

To make it less diverse, separate inline comments by exactly two spaces.

## Blank Lines

[PEP 8 section Blank Lines][]:
> Surround top-level function and class definitions with two blank lines.
>
> Method definitions inside a class are surrounded by a single blank line.

To make it less diverse, adhere only to these points from that section.

## Special Characters

Only `\n` is allowed as line separator. Only spaces are allowed for indent.

No control characters are allowed.

## Type Hinting

Type hinting format is adopted from the PEP 484 and, specifically, from the
[PEP 484 section Suggested syntax for Python 2.7 and straddling code][].

Type hinting is allowed only in comments, not in annotations. Nevertheless, symbols used in type
hints must be imported or defined in surrounding code. Special symbols for type hinting are
imported from `typing` package.

Type hints are not required but must always be correct. I.e. if new argument added to function that
has a type hint, type hint must be changed accordingly.

### Signature on a Single Line

If type hint fits [maximum line length](#maximum-line-length), type hinting comment must be on the
same line as signature:
```python
def add(a, b):  # type: (int, int) -> int
    return a + b
```

Otherwise, it should be on separate line:
```python
def embezzle(account, funds=1000000, *fake_receipts):
    # type: (str, int, *str) -> None
    """Embezzle funds from account using fake receipts."""
    # <code goes here>
```

### Multiline Signature

Type hint can be either on separate line:
```python
def embezzle(
        account,
        funds=1000000,
        *fake_receipts):
    # type: (str, int, *str) -> None
    """Embezzle funds from account using fake receipts."""
    # <code goes here>
```

Or, if arguments are written one per line, following form may be used:
```python
def send_email(
        address,  # type: Union[str, List[str]]
        sender,  # type: str
        cc,  # type: Optional[List[str]]
        bcc,  # type: Optional[List[str]]
        subject='',
        body=None  # type: List[str]
):
    # type: (...) -> bool
    """Send an email message. Return True if successful."""
    # <code goes here>
```


## Documentation

[Doxygen][] is used as documentation engine. [doxypypy][] is used as its preprocessor.

Documentation of modules, classes and functions should be in docstrings. Docstrings should be
formatted according to PEP 8 and PEP 257 with [Doxygen Special Commands][] prefixed with `@`
(not a backslash).
```python
def documented_function():
    """Short, one-line description."""
```
```python
def documented_function():
    """Short, one-line description.
    @return Nothing meaningful.
    """
```
```python
def documented_function():
    """@return Nothing meaningful."""
```
```python
def documented_function():
    """Long, very long and wordy multiline description, which is not going to fit one line; the
    only purpose of it is to show the correct way to format multiline docstrings.
    """
```

If textual description in docstring is multiline, add a single blank line
before parameters/return description.
```python
def documented_function():
    """Long, very long and wordy multiline description, which is not going to fit one line; the
    only purpose of it is to show the correct way to format multiline docstrings.

    @return Nothing meaningful.
    """
```

If description of parameter/return is multiline, second and following lines should
have extra 4-spaces indent.
```python
def documented_function_with_args(first_arg, *args, **kwargs):
    """Long, very long and wordy multiline description, which is not going to fit one line; the
    only purpose of it is to show the correct way to format multiline docstrings.

    @param first_arg Positional argument, which has very long description which is not going to
        fit one line and, therefore, should be indented by 4 spaces more starting with second line.
        Third and following lines are indented by 4 spaces too, of course.
    @param args Extra args.
    @param kwargs Simple kwargs.
    @return Nothing meaningful but having a very long description that is not going to fit one line
        again only to show how to format long descriptions of parameters and return values.
    """
```

### Module Documentation

If the module has a documentation, it should start with package declaration. Otherwise, it will not
be visible in generated documentation. In `top/nested/module.py` it should be:

@code{.py}

"""@package top.nested.module
The long and lengthy, verbose and protracted but with no purpose except
serving as documentation example.
"""

def foo():
    pass


def bar():
    return 'buzz'


class Qux(object):
    pass

@endcode

### Class and Object Attributes

First, when documenting an object attribute, consider transforming attribute into a getter or a
property: they can have a native Python docstring, which are standard and supported by multitude of
tools.

When deemed necessary, write a comment starting with `##` above. Add a blank line between commented
attribute and adjacent statements.
```python
class Example(object):
    ## Documented class attr with very-very long documentation, the only
    # purpose of which is to show how to write documentation for attributes
    # to make them available in documentation.
    class_attr = 'hi'

    ## Documented class attr with one line of documentation.
    another_class_attr = 'hello'

    def __init__(self):
        ## Documented object attr with one-line documentation.
        self.attr = 'lo'

        total = self.class_attr + self.another_class_attr + self.attr

        ## Another documented object attr with one-line documentation.
        # It has more than one line.
        self.another_attr = total * 2
```

[PEP 8]: https://www.python.org/dev/peps/pep-0008/
[PEP 8 section Inline Comments]: https://www.python.org/dev/peps/pep-0008/#inline-comments
[PEP 8 section Blank Lines]: https://www.python.org/dev/peps/pep-0008/#blank-lines
[PEP 257]: https://www.python.org/dev/peps/pep-0257/
[PEP 484]: https://www.python.org/dev/peps/pep-0484/
[PEP 484 section Suggested syntax for Python 2.7 and straddling code]:
https://www.python.org/dev/peps/pep-0484/#suggested-syntax-for-python-2-7-and-straddling-code
[C++ Coding Style]: https://networkoptix.atlassian.net/wiki/spaces/SD/pages/44531791
[Doxygen]: http://www.stack.nl/~dimitri/doxygen/
[Doxygen Special Commands]: https://www.stack.nl/~dimitri/doxygen/manual/commands.html
[doxypypy]: https://github.com/Feneric/doxypypy
