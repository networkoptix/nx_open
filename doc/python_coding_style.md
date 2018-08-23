# Python Coding Style

Since there exist widely accepted
[PEP 8](https://www.python.org/dev/peps/pep-0008/) (general style) and
[PEP 257](https://www.python.org/dev/peps/pep-0257/) (docstrings),
this coding style is built upon them.
All PEP 8 and PEP 257 restrictions still holds.

In some cases new restrictions are imposed:
- to comply with
[C++ Coding Style](https://networkoptix.atlassian.net/wiki/spaces/SD/pages/44531791),
- to follow the style of the existing code.

To remove burden of manual reformatting,
this coding style is supported by most tools (pylint, PyCharm inspections)
with limited number of non-default settings.

Below are only:
- additional restrictions and refinements to PEP 8,
- differences from C++ Coding Style.

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

When closing parenthesis/bracket/brace is on the separate line, only the
following style is used:
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

If closing parenthesis/bracket/brace is on the separate line, last element
should be followed by comma. Otherwise, it should not.

## Inline Comments

[Inline Comments](https://www.python.org/dev/peps/pep-0008/#id32)
section of PEP 8:
> An inline comment is a comment on the same line as a statement. Inline
comments should be separated by at least two spaces from the statement.
They should start with a `#` and a single space.

To make it less diverse, separate inline comments by exactly two spaces.

## Blank Lines

[Blank Lines](https://www.python.org/dev/peps/pep-0008/#id21)
section of PEP 8:
> Surround top-level function and class definitions with two blank lines.
>
> Method definitions inside a class are surrounded by a single blank line.

To make it less diverse, adhere only to these points from that section.

## Special Characters

Only `\n` is allowed as line separator. Only spaces are allowed for indent.

No control characters are allowed.

## Documentation

[Doxygen](http://www.stack.nl/~dimitri/doxygen/) is used as documentation
engine. [doxypypy](https://github.com/Feneric/doxypypy) is used as its
preprocessor.

Documentation of modules, classes and functions should be in docstrings.
Docstrings should be formatter according to PEP 8 and PEP 257 with Doxygen
[Special Commands](https://www.stack.nl/~dimitri/doxygen/manual/commands.html)
prefixed with `@` (not a backslash).
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
    """Long, very long and wordy multiline description, which is not going
    to fit one line; the only purpose of it is to show the correct way to
    format multiline docstrings.
    """
```

If textual description in docstring is multiline, add a single blank line
before parameters/return description.
```python
def documented_function():
    """Long, very long and wordy multiline description, which is not going
    to fit one line; the only purpose of it is to show the correct way to
    format multiline docstrings.

    @return Nothing meaningful.
    """
```

If description of parameter/return is multiline, second and following lines should
have extra 4-spaces indent.
```python
def documented_function_with_args(first_arg, *args, **kwargs):
    """Long, very long and wordy multiline description, which is not going
    to fit one line; the only purpose of it is to show the correct way to
    format multiline docstrings.

    @param first_arg Positional argument, which has very long description
        which is not going to fit one line and, therefore, should be
        indented by 4 spaces more starting with second line.
    @param args Extra args.
    @param kwargs Simple kwargs.
    @return Nothing meaningful but having a very long description that is
        not going to fit one line again only to show how to format long
        descriptions of parameters and return values.
    """
```

### Module Documentation

If the module has a documentation, it should start with
`@package path.to.module`. Otherwise, it will not be visible in generated
documentation. E.g. if `top/nested/module.py` has a docstring, it should be:
```python
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
```
