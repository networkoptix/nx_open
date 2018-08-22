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

The only style that comply with PEP 8 and C++ Coding Style is:
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

In existing code base, when closing parenthesis/bracket/brace is on the
separate line, only the following style is used:
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

Closing brace has the same indent as first parameter/argument.

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
