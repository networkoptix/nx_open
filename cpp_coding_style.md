# C++ Coding Style

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

This guide covers mainly syntactical aspects of C++ code. It does not intend to cover coding
practices such like the choice of language/library features.

The Main Goal is to **minimize efforts for reading** and help **unambiguous understanding** of the
code. The Style also helps to achieve the **clarity of intentions** and **consistency in expressing
similar intentions.**

General recommendations:

- Do not save **keypresses** when writing code.

- Format your code to be **clear** and **consistent,** rather than **beautiful.**

- Do not try to make the code just **shorter,** unless it aligns with the Main Goal.

- Avoid **tautology** like commenting what is obvious from the code or the identifiers.

Treat this guide like a **tone-setting showcase** rather than a complete formal definition of the
code formatting rules. **Be loyal** to its ideas even if you don't like some - your cooperation
is essential for our success.

**Unit tests** are not an exception for the coding style Main Goal, but in the case they require
some specific code not usually found outside unit tests, it can be formatted with exceptions from
this Coding Style.

When changing the **existing code:**

- When making **any changes** to a **third-party file, preserve** its original coding style.

- When making **small changes** in an existing in-house file, **preserve** its existing coding
    style, unless it is mainly inconsistent by itself.

- The **smallest entity** of applying a consistent coding style is a multiline declaration or
    definition at the file's top level not considering namespaces - typically, a **class, enum or
    function definition.**

- When **significantly changing** a multiline "smallest entity", e.g. close to or more than a
    half of the lines, **reformat** the whole entity to the new coding style.

- When **significantly changing a file,** e.g. close to or more than a half of the lines,
    **reformat** the whole file to the new coding style.

LEGEND: In the code examples below, **comments** following a dollar sign **`$`** are part of this
guide rather than of the example code.

IMPORTANT: Feel free to **break a rule**, if it makes your code better serve the Main Goal.

Happy coding!

---------------------------------------------------------------------------------------------------
## Low-level file structure

### File and folder names

- ASCII printable only.

- No Windows-incompatible chars (`\/:*?"<>|`) and names (`aux`, `con`, `prn`, `nul`, `com#`,
    `lpt#`, where `#` is a digit `0`..`9`).

- No spaces.

- Lower case, words separated with underscores (`_`), not hyphens (`-`).

### Text file contents

- **Unix newlines** (U+000A).

- **No tabs,** no other **control chars.**

- **No** line **trailing** **spaces.**

- A **newline** at the **end** of a **file** (except empty files).

- **No trailing and leading empty lines.**

- **No** two or more **successive empty lines.**

- **ASCII** chars only.
    - NOTE: All our C++ compilers are set up to assume **UTF-8 for string literals,** so
        non-ASCII chars can be included simply by `\u`, without any need for `u8"`:
        ```cpp
        constexpr char kZhe[] = "\u0416"; //< CYRILLIC CAPITAL LETTER ZHE.
        ```
        The meaning of such chars must be explained in the comments.

### Indentation

- Each **indentation** level is **4 spaces.**

- When breaking long lines, indent **continuation** lines **4 spaces** more than the first line.

- **No** two or more **successive spaces,** except in indents, literals and column-aligned text
    (where the latter is allowed).

- Always **indent** lines with a **multiple of 4: never align** the start of the line to a
    particular **upper line** part.

- The indentation may be omitted for long string literals like CLI tool help text.

### Line length

- The line length limit is **99 characters** (borrowed from Python PEP 8 and Qt Coding Style, but
    no extra limit is set for comments).

- Long **horizontal separators** (e.g. `//---`...) should span up to column 99.

- A long string literal which is hard to break is a valid excuse, while a long comment is not.

- When defining a value for a byte array, e.g. a UUID, **removing spaces** to fit into one line
    is a **better** alternative to making the line longer than allowed:

    **UUID constant definition**
    ```cpp
    static const Uuid IID_TimeProvider = //< {8118AE76-37A1-4849-83E2-54C11EBF5A22}
        {{0x81,0x18,0xAE,0x76,0x37,0xA1,0x48,0x49,0x83,0xE2,0x54,0xc1,0x1E,0xBF,0x5A,0x22}};
    ```

### File and function length

- Limit **function** definitions to be no longer than **50 lines** - this typically fits into
    one screen. If a function is longer, **decompose** it.** **When adding to an **existing
    function** which is **already longer** than 50 lines, **decompose** it **if added ~25
    lines** or more.

- Limit **files** to no longer than **1000 lines.** If a file is longer, **decompose** it. When
    adding to an **existing file** which is **already longer** than 1000 lines, **decompose** it
    **if added ~500 lines** or more.

- Prefer **class definitions** no longer than **50 lines.** If a class is longer, **decompose**
    it, considering the **Pimpl** design pattern if the **private** part is the issue.

### Column-aligned text

- **Do not use column-aligned text** in regular code, e.g. in variable declarations or function
    arguments.

- Column alignment may be used e.g. in table-like array initializers in unit tests.

---------------------------------------------------------------------------------------------------
## Unit file structure, includes and namespaces

### Regular C++ unit

#### `some_unit.h`

```cpp
#pragma once //$ First line of a .h file. Do not use #define for include guards.

/**@file
 * Mechanism for performing a certain job.
 * $ Place a Doxygen block here if needed to document the entire file, followed by an empty line.
 * $ Use a wording which names the concept or entity represented by this file, without words
 * $ "this file" or the like. Alternatively, use the function-style wording, if the main function
 * $ of the file is more of a job rather than a thing.
 * $ If the file defines a single main entity (e.g. a class), use its Doxygen comment instead.
 */

//$ Explicitly include headers for all symbols that are used in the current file, even if such
//$ headers are already included indirectly, except for those included in the own header of the
//$ .cpp file.
//$
//$ Put an empty line after each "#include" group. Sort headers lexicographically in each group.

#include <standard_headers>

#include <thirdparty_like_Qt_and_Boost>

extern "C" {
#include <libavcodec/avcodec.h> //$ Inside extern "C", includes are not indented.
} // extern "C"

//$ Use full paths in angle brackets if the included file is not conceptually closely tied to the
//$ current file.
#include <nx/project/headers>

//$ Use relative paths in quotes if the included file is conceptually closely tied to the current
//$ file. In particular, this means that in case of moving the current file, the included files
//$ most likely will be moved together with it as a whole.
#include "sibling.h"
#include "../common_interface.h"

//$ Forward declarations with namespaces can use single-line form:
namespace nx { class NxTool; }
namespace nx { namespace common_tools { class CommonTool; } }

namespace nx::some_library { //$ Use chained namespaces if C++ version allows.

//$ Namespace definition and its contents, including inner namespaces, is not indented.
//$ Put empty lines around a namespace, unless it is solely enclosed in another namespace.

//$ To mark symbols exported from the library, use NX_..._API macros defined in the build system.

class NX_SOME_LIBRARY_API PublishedClass
{
    nx::common::api::Error setValue(int value); //$ Use full namespaces in headers.
    ...
};

NX_SOME_LIBRARY_API void publishedFunction();

} // namespace nx::some_library
```

#### `some_unit.cpp`

```cpp
#include "some_unit.h" //$ First line of a cpp is the unit's own ".h" in quotes.

#include <standard_headers> //$ Put an empty line after each "#include" group.

#include <thirdparty_like_Qt_and_Boost>

#include <nx/project/headers>

namespace nx::some_library {

//$ Synonyms: use them only in .cpp files, and only when it helps readability.
//$ Introduce synonyms at namespace scope only when they are useful in many functions; otherwise,
//$ introduce them at function scope. Do not introduce new identifiers as aliases to differently
//$ named entities at namespace scope for the purpose of reducing the amount of code.
using nx::common::SomeTool; //$ Prefer using a needed symbol to using the whole namespace.
using namespace nx::common::tools; //$ Use only when many symbols from a namespace are needed.
using analytics = nx::vms::sdk::analytics; //$ Use only with an identical synonym name.
using Error = nx::common::SomeTool::Error; //$ Use only with an identical synonym name.

//$ Type aliases that introduce new concepts are not considered synonyms. They have to be used
//$ consistently, without mixing with their original entities.
//$ Prefer "using" to "typedef", except for function pointers.
using PublishedClassPtr = std::shared_ptr<PublishedClass>;

namespace {

//$ Unnamed namespace must be used in .cpp for non-published types, and also can be used for
//$ `static` global functions and variables (which still have to be declared `static`) when they
//$ are located together with non-published types. Unnamed namespace can be re-opened multiple
//$ times where needed, though generally its recommended to use one unnamed namespace at the
//$ beginning.

static int g_privateGlobalVar;

constexpr int kPrivateConst = 113;

class LibraryPrivate
{
    ...
};

static void privateFunction()
{
    ...
}

static inline void privateInlineFunction()
{
    ...
}

} // namespace

//$ Standalone `static` variables and functions do not have to be wrapped into an unnamed
//$ namespace.
static int g_debugCounter = 0;

PublishedClass::PublishedClass() //$ No NX_..._API macro here.
{
    ...
}

void publishedFunction() //$ No NX_..._API macro here.
{
    ...
}

} // namespace nx::some_library
```

### Unit with templates

#### `processor.h`

```cpp
#pragma once

namespace nx::some_library {

namespace processor_detail { //$ Implementation details of the unit, accessible from this header.

//$ If a namespace with non-published symbols needs to be accessed from the header because e.g.
//$ some helper methods are called from templates, the namespace should be named with unit name
//$ and "_detail" suffix. Unit name can be omitted if it is the only unit in the namespace, or
//$ if it seems reasonable to share the same "_detail" namespace between all namespace units.

void processString(const QString& value); //$ Helper, cannot be moved to cpp: used from a template.

} // namespace processor_detail

constexpr int kCount = 113;

template<class Data>
void process(const Data& data) //$ Public function template, definition cannot be moved to cpp.
{
    const int kCount = 113;
    QString s = serialized<Data>(data);
    processor_detail::processString(s);
}

void processFixedData(); //$ Public function - not a template, definition is in cpp.

} // namespace nx::some_library
```

#### `processor.cpp`

```cpp
#include "processor.h"

namespace nx::some_library {

namespace processor_detail { //$ Cannot be unnamed: accessed from templates in the header.

void processString(const QString& value)
{
    ...
}

constexpr int kMaxStringCount = 1000; //$ Some other definition, local to this cpp.

} using namespace processor_detail; //$ Explicitly mimic unnamed namespace behavior.

void processFixedData()
{
    processString("fixed_data"); //$ No need for "processor_detail::" prefix because of "using".
}

} // namespace nx::some_library
```

### Unit with conditional body and/or includes

#### `some_unit.h`

```cpp
#pragma once
#if defined(ENABLE_XXX) //$ The whole unit is conditional: empty line after, no empty line before.

//$ Conditional #include. The whole #if structure is regularly indented.
#if !defined(DISABLE_FFMPEG)
    #include ...
#endif

...

#endif // defined(ENABLE_XXX) //$ Duplicate #if condition. Last line of a cpp; empty line before.
```

#### `some_unit.cpp`

```cpp
#include "some_unit.h"
#if defined(ENABLE_XXX) //$ The whole unit is conditional: empty line after, no empty line before.

#include ...

...

#endif // defined(ENABLE_XXX) //$ Duplicate #if condition. Last line of a cpp; empty line before.
```

---------------------------------------------------------------------------------------------------
## Identifiers

### Wording

- Use **single-letter** identifiers **only in short scopes** which are a few lines long. Avoid
    single-letter identifiers `l`, `o` and `O` due to similarity with digits.

- Use only **correct American English** words and abbreviations (as defined below), with
    proper capitalization depending on the identifier purpose.

- If an identifier **clashes** with a **reserved word,** first try to find a **synonym,** and if
    not possible, use the word with a **trailing `_`,** e.g. `enum class HttpMethod { get, put,
    delete_ }`.

- Do **not** invent **compound words** gluing adjacent words together, though you may use
    well-established in IT compound words like `filename`, `homepage`, `touchscreen`, `fullscreen`,
    and the like.

- Do **not** assign a **special meaning** or aspect to **common words,** unless it is a term of a
    domain; instead, add clarifying words. E.g., for thread-safety/unsafety, use
    `ThreadSafe`/`ThreadUnsafe` suffix to variables and methods, instead of just `Safe`/`Unsafe`.

- **Capitalize** all abbreviations and acronyms **as regular words,** e.g. `Xml` and `Url` (when
    capitalization is needed), and `xml` and `url` (when lower case is needed).

- Do **not** use **ambiguous abbreviations and acronyms,** which can be easily understood in
    multiple ways, e.g. `cnt` (`count` or `center`?), `rq` or `req` (`request` or `requirement`?),
    `lst` (`list` or `last`?), `res` (`result`, `resolution`, `restore`, `reset`, `response`, or
    `resource`?).

- Use only a limited number of **well-established** non-ambiguous **abbreviations,** e.g. `ptr`,
    `src`, `impl`, `param`, `var`, `min`, `max`, `str`, `err`.

- To name **measurement units,** use either **standard** names like SI (e.g. `ms` for milliseconds
    and `us` for microsecond) or **full words**, avoid abbreviations like `sec` (`second` or
    `sector`?) and `min` (`minute` or `minimum`?). For the greek `mu` prefix for _micro_, use `u`.
    Always use **unit postfix** for identifiers which denote time and other measurements, e.g.
    `timeSeconds`, `delayMs`, or `sleepUs`, unless the unit is absolutely clear from the context,
    and **except** when the library, e.g. **std::chrono**, guarantees safety.

- The name of a **type** or a **variable** should be a **noun,** possibly prefixed with adjectives.

- The name of a **function** should normally start with a **verb,** but can be a **noun** if it is
    more of calculating a **formula** rather than performing a **job.**

- **Avoid** words which imply **apostrophes**; use their equivalents like `cannot` (as a single
    word) or `doNot`.

- Use the **`set`** prefix for **setters.**

- Use **`is`**, `are`, `has`, `need`, or another auxiliary verb as a prefix for boolean
    **getters.**

- Use **`is`**, `are`, `has`, `need`, or another auxiliary verb as a prefix for boolean
    **variables** and **functions** only if their meaning is **not clear** otherwise.

- **Omit** the **`get`** prefix from **getters** unless it is needed for consistency with other
    code, e.g. Qt.

### Typical mistakes

- **read~~ed~~, build~~ed~~, bind~~ed~~,** etc. - incorrect verb forms. Use **read** (with
    additional words if needed for clarity), **built**, **bound,** etc. Note that for verbs that
    have evidence of participating in the **regularization trend,** the regular (-ed) form is
    allowed; this includes **casted**.

- **A cleanup, a setup, a workaround,** etc. (single word) are **nouns**, while **to clean up, to
set up, to work around,** etc. (two words) are **verbs**. Use either a single-word or a double-word
form depending on whether you need a noun or a verb.

- **data~~s~~, info~~s~~,** etc. - these words have no plural form. Use additional words, e.g.
    **dataList.**

- **matri~~x~~es, verti~~x~~es,** etc. (except **indexes** which is legal and preferred over
    **indices**) - correct plural forms are **matrices, vertices,** etc.

- **object~~s~~Properties, item~~s~~Count** and the like - nouns used to define the **plural-form**
main noun should have **singlular** form: **objectProperties, itemCount.**

- **ID** - can be all-caps only in all-caps identifiers, otherwise, should be **Id** or **id.**

- Avoid typical typos:
    - prefe~~r~~ed - preferred
    - transfe~~r~~ed - transferred
    - ph~~i~~sical - physical
    - respon~~c~~e - response
    - anal~~i~~ze - analyze
    - authent~~ific~~ation - authentication
    - depr~~i~~cated - deprecated
    - du~~b~~licate - duplicate
    - pre~~ff~~ix - prefix
    - auxi~~la~~ry - auxiliary
    - conver~~t~~ion - conversion
    - occu~~r~~ed - occurred
    - param~~i~~ter - parameter
    - delim~~e~~ter - delimiter
    - correspond~~ent~~ - corresponding

- Use American English word forms where applicable, including:
    - art~~e~~fact - artifact
    - analy~~s~~e - analyze
    - cance~~ll~~ed - canceled
    - behavio~~u~~r - behavior
    - colo~~u~~r - color

### Letter case

For **enums,** see a dedicated section of this Guide.

```cpp
namespace my_tools {

struct MyData
{
    int someValue;
};

class SomeData
{
    static constexpr int kSomeValue = 113;
    static constexpr const char* kAsciiLiteral = "AsciiLiteral";

    //$ Where possible, initialize plain and value-holding types with `=` instead of `()` or `{}`.
    static const std::string kName = "some name";

    static int s_itemCount;
    int m_someValue;
    void someMethod();
};

using OptionalStr = boost::optional<std::string>;

/**
 * $ Instead of a class with static-only members, use a namespace named and formatted like a class,
 * $ with a Doxygen comment if needed. Such namespace should be defined in a single unit (.h/.cpp).
 */
namespace LogHelper
{
    void logValue(int value);
}

constexpr int kMyNumber = 113; //$ Constant defined in any scope.

int g_someGlobalVar; //$ Global var - declared in a header, defined in a cpp.

static int g_callCount = 7; //$ Unit-global var - declared in and local to a cpp.

void someFunction(int someParam, int* outSomeNumber);

} // namespace my_tools

#define LOG(VALUE) log((VALUE), __LINE__)
```

### Typical names

- `ResultCode` - type (typically `enum class`) for a result code of some operation or protocol.

- `result` - a variable which stores the value to be returned from the current function.

- Short names; allowed only in **short local scopes:**
    - `r` - a variable which stores the result of some other function.
    - `s` - a string.
    - `c` - a char.
    - `i` - a variable typically used as a `for` counter.
    - `t` - Timer or captured current time.
    - `d` - Pointer to private object in the \"pimpl\" design pattern.
    - `ui` - Class member pointer to the instantiated widget form class generated by Qt User
        Interface Compiler (uic).
    - NOTE: `d` and `ui` are two common exceptions to the general rule of class member field naming
        which requires the `m_` prefix.

- `ItemCount` - use the postfix `Count` to denote the number of items instead of e.g. `number`,
    `num` or `n`. Note the **singular form** of the noun `Item`.

- `outIndex` - use the prefix `out` for function out-parameters.

- `lock` - a local variable which represents a locked mutex.

- `parent` - an owner of the current object.

- `it` - an iterator.

---------------------------------------------------------------------------------------------------
## Comments

### Comments for code

Use only **complete** and correct **American English sentences** starting with a capital letter and
ending with `.`, `?` or the like. Use only correct **American English words** and
**abbreviations**, guided by the same rules as for identifiers. Use proper American English
**grammar with articles, punctuation** and proper spacing. Use ` - ` (space-hyphen-space) for a
**long dash,** `"` (double quote) for natural language quotes, **backticks** for quoting a literal
text like a code fragment or file name, **single space** between sentences. Use `!`, `!!!`, `...`
only with exactly 1 or 3 repeating characters and only when really necessary. Do not end a comment
with `:`, unless really necessary.

Capitalize as regular words and treat as a single word (without hyphens) the words **internet,
email, newline, multiline,**, as well as **iframe** (to avoid confusion with an interface when
capitalized) and **id** (except when it stands for "Identification Document"). Follow the
**trademark** style for `iPhone` and the like. Use commonly accepted letter case for abbreviations
and acronyms; alternatively, use identifier-style letter case (like of a regular word) for them.

For the word **todo**, use the letter case `ToDo` or `todo` when referring to a ToDo concept rather
than writing a todo note itself, where it must be written in all-uppercase.

Also apply the above rules to a non-localized program output, error and log messages.

A single C++ identifier, a word, or a short word combination can form a non-Doxygen comment (thus,
without capitalization and a period), where it looks more appropriate.

In a paragraph, **break a line** only when the line **width is exhausted,** like in a book. Use an
empty line (prefixed with `//` or `*` when needed) between paragraphs.

**Directly reference** C++ **identifiers,** without quotes or markup/tags, when they do **not cause
confusion** by being mistaken for regular words; otherwise, surround them with **backticks**.

**Do not refer** to the particular **developers** except in todos.

```cpp
// Regular comment refers to the lines below. Separate from "//" with a space. Prefer "//" to "/*".

// Multiline comment: each line
// starts with two slashes.

/* C-style comment has spaces at the ends, except when shown otherwise. Use single line only. */
/* C-style comments should be used only when necessary, e.g. in macros or inside constructs. */

doSomething(); //< Always start a comment with `<` when it refers to the preceding code.

doJob(); /*< C-style comment also starts with `<` when it refers to the preceding code. */

//$ Colon and a space always follow `TODO`.
// TODO: #developer Things to do, addressed to the developer.
// TODO: Things to do without assigning a developer.
// TODO: Other #developers can be mentioned in the ToDo text.
call(); //< TODO: Comment starting with `<` refers to the preceding code.
```

### Commenting out code

For alternative implementations and to-be-decided-if-needed parts **prefer runtime choice** via a
dedicated configuration mechanism or a bool constant.

If **compile-time switching** is required (e.g. to avoid dependencies), use:

```cpp
    #if !defined(FLAG) //$ Indented as regular code; no spaces after `#`; do not use #ifdef/#ifndef.
         #if defined(__arm__)
             doSomething();
         #endif //$ Not duplicating #if condition, because #-directives are regularly indented.
    #else
        doSomethingElse();
    #endif
```

For other cases of commented-out code, add a comment telling **why** the code is **commented out**
(unless absolutely obvious), and use:

```cpp
void f()
{
#if 0 // Explanation why this code is commented out. //$ Such directives are not indented.
    doSomething(); //$ Originally indented single-line or multiline code.
#endif // 0 //$ Duplicating #if condition, because #-directives are not indented.
    ...

    //$ Less preferable, only for single-line code:
    //SingleLineCode(); //$ No space after `//`; `//` starts with the original code indent.
}
```

### Sections and horizontal separators

```cpp
//$ Below is a horizontal separator at the top level of a file (outside a function or class). Its
//$ last `-` is in the column 99. An empty line is placed before and after the separator.

//-------------------------------------------------------------------------------------------------

//$ Below is a named section header in a file. An empty line is placed before and after the header.

//-------------------------------------------------------------------------------------------------
// Name of the section - a complete sentense ending with a period; do not capitalize each word.
```

### Function arguments

```cpp
//$ Comment out unused arg names. Note the spacing.
void render([[maybe_unused]] Frame frame, int /*number*/)
{
    #if !defined(DISABLE_RENDER)
        doRender(frame);
    #else
        //$ Use [[maybe_unused]] in C++17 if commenting out arg name is not possible. Use a
        //$ commented C-style cast to void if [[maybe_unused]] is not supported.
        /*unused*/ (void) frame;
    #endif

    //$ Named args - use if otherwize the value meaning is not obvious. Note the spacing.
    renderFrame(frame, /*left*/ 0, /*top*/ 100, /*pts*/ 0);
}

//$ When an unused arg name can be clearly deduced from its type (like `request` here),
//$ its commented-out occurrence can be omitted.
void process(const Request&, int* /*outHttpStatus*/) override
{
}

//$ If necessary, specify the default value in a function definition in a comment; beware the risk
//$ of divergence. Note the spacing.
void myFunction(int arg, Helper* helper /* = nullptr*/)
{
    ...
}
```

### Doxygen

To enable generating code docs, we use Doxygen-compatible comments, using Javadoc syntax where
possible. Even for private entities and in .cpp files we use Doxygen comments for consistency.

```cpp
/**@file
 * Such block describes the whole file. Use only if the file contains multiple main public entities
 * like functions or classes, and needs a description as a whole. Followed by an empty line.
 */

//$ - Use only `/**` to start a Doxygen comment.
//$ - Use only `@` for tags.
//$ - Do not start text with "This class...", "This function..." and the like.
//$ - Do not use column-aligned text.
//$ - Do not use the "brief" feature: neither count on auto-briefing (on the first period), nor use
//$     `@brief`.
//$ - Do not put two spaces between sentences.
//$ - Avoid using rare tags like `@note`, `@see`, `@ref` and `{@link}` - make use of plain text
//$     instead.

//$ Function comment should begin with the description of what this function does, without the
//$ words "this function" or the like, in the indicative mood (like "Performs the job...").
//$ Arguments that are referenced in this comment should come plain, without quotes, and without
//$ articles (this helps to distinguish them from generic words).

/**
 * Processes arg and performs necessary actions.
 * @param arg Complete sentence(s), started capitalized, finished each with a period. If there are
 *     continuation lines, indent them with 4 spaces.
 *     $ Refer to other C++ symbols and params directly by their names, like OtherClass or
 *     $ anotherParam, without quotes and without articles. Indent continuation lines with 4 spaces
 *     $ after `*`.
 * @return Complete sentence(s), started capitalized, finished each with a period.
 *     $ Use true, false and null as regular English words; use nullptr, NULL and the like only
 *     $ when absolutely necessary. Indent continuation lines by 4 spaces after `*`.
 * @throws MyError Starts capitalized. Describe conditions which lead to the exception.
 */
Result function(Type arg);

/**
 * Long multiline function comment. Separated from the following Doxygen tag (if any) by an
 * asterisk-only line. The continuation lines should not be indented.
 *
 * If there are multiple paragraphs, they should be separated by an asterisk-only line.
 *
 * Lists:
 * - Use `-` bullets.
 *     - Inner lists should have the same `-` bullets and be indented with 4 spaces.
 * - Indent continuation lines with 4 spaces. Use either complete sentences (capitalized, finished
 *     with a period), or, if short, word combinations:
 *     - without a period
 *     - non-capitalized
 *
 * @return Some number.
 */
int function();

/**
 * Tool which provides some numbers.
 * $ Class comment should begin with the detailed name of a concept or entity it represents,
 * $ without the words "this class" or the like.
 */
class MyTool
{
public:
    /**
     * Public field of a non-private (outside of unnamed namespace) class should have a Doxygen
     * comment.
     */
    int m_number;

    int getNumber1(); /**< For single-line cases trailing comments can be used. */
    int getNumber2(); /**< For single-line cases trailing comments can be used. */

    /**
     * Use `code` HTML tags for long inline code; do not use it for a single C++ symbol reference:
     * <code>digest = md5(name + ":" + realm + ":" + password).toHex();</code>
     * Additionally wrap in <pre></pre> if the code snippet is multiline:
     * <pre><code>
     *     uint64_t digest = calsDigest(userName);
     *     connect(digest);
     * </code></pre>
     */
    uint64_t m_digest;

public:
    /** Single-line comment without `@`-tags, allowed for a single-line entity only. */
    void method();

private:
    int m_int; /**< Comment for a private entity must also be Doxygen-formatted. */
}

/** Macro comment has the same style as a function comment. */
#define NX_DEBUG_LOG(...) NX_LOG(__VA_ARGS__, cl_logDEBUG2)

//$ Deprecated entity. Besides the @deprecated tag, do not tell explicitly that the entity is
//$ deprecated. After the tag, consider explaining why is it deprecated and what to use instead.
/**
 * Does the job.
 * @deprecated Is not thread-safe. Use threadSafeJob() instead.
 */
void doJob();
```

### Apidoc

For generating the HTTP API documentation, we use an in-house tool `apidoctool` written in Java,
which parses C++ comments called "apidoc comments". Where possible, they inherit the style of
Doxygen comments, with the main difference that they use `%` instead of `@`, and start with
`/**%apidoc[...]` (here `[...]` stands for any properties which are attributed to the `%apidoc`
tag). As with Doxygen comments, apidoc comments can either take a full multiline form, or be
single-line. Another difference is that semantically inner tags (like `%value` inside `%param`) are
indented by 4 spaces relatively to their parent tag (in Doxygen there is no concept of tags being
inner to parent tags).

ATTENTION: Here we only define the formatting of Apidoc comments, not their semantic structure.
Refer to the `apidoctool` built-in Javadoc documentation for the description of Apidoc comment
language.

```cpp
struct User
{
    //$ ATTENTION: The comment `User type` below is NOT a tautology in case of apidoc, because the
    //$ struct name is eliminated from the API documentation.
    /**%apidoc[opt]:enum
     * User type.
     * %value local This user is managed by the Server. Session tokens must be obtained
     *     locally.
     * %value ldap This user is managed by the LDAP server.
     */
     UserType type = UserType::local;

    /**%apidoc Single-line apidoc comment. */
    bool setCookie = false;
};

void registerHandlers()
{
    /**%apidoc GET /ec2/getMediaServerUserAttributesList
     * Read additional Server attributes.
     * %param[default] format
     * %param[opt]:uuid id Server unique id. If omitted, return data for all servers.
     * %param[proprietary]:string typeId Should have fixed value.
     *     %value {f8544a40-880e-9442-b78a-000000000000}
     * %return List of objects with additional server attributes for all servers, in the requested
     *     format.
     *     %struct MediaServerUserAttributesDataList
     */
    regGet<QnUuid, MediaServerUserAttributesDataList>(f, p,
        ApiCommand::getMediaServerUserAttributesList);
}
```

---------------------------------------------------------------------------------------------------
## Spaces, empty lines and curly braces

### Spaces

Use regular punctuation spacing: a space after, and no space before `,`, `;`, and `:` except in the
ternary operator `?:`.

```cpp
//$ Space after the keyword only before opening parentheses and other words, except in operators
//$ and function-like constructs:
template<...>
static_cast<...>()
operator+(...)
operator()(...)
operator new(...)
sizeof(...)
typeid(...)
alignof(...)
noexcept(...)
new(...)
new int(...)
if (...)
if constexpr (...)

return -value; //$ No space after unary operators, except the C-style type cast.
return item.size * list->count(); //$ Space around binary operators except `,`, `.` and `->`.
add(str[i]); //$ No space inside parentheses and brackets.
void func(int arg) //$ No space after a function name before `(`.
(int) v.size() //$ A space after the C-style type cast.

//$ "*" and "&" stick to the type, space before the identifier, `const` spaced on both sides.
//$ Declare each variable on a dedicated line.
constexpr const char* kText = "text";
for (auto& item: collection)...
void (*alloc)(size_t size); //$ In a function pointer, no space between `*` and the identifier.
```

### Empty lines

- No empty lines after an opening and before a closing curly brace.

- An empty line between multiline (the count includes comments) declarations or definitions in a
    class, namespace, or file.

### Curly braces

- In `if`, `while`, `for` and the like, use curly braces around a single **inner multiline
    instruction**, and also when the **part in `()` is multiline** (even if the inner instruction
    is single-line).

- In **single-line** constructions except Uniform initialization (Initializer list), use a
    **space** after `{` and before `}`.

- Curly braces in classes, enums and blocks are **not additionally indented** and typically
    occupy **dedicated lines** except in namespaces, `extern "C"` and `do...while`.

- **Empty** curly braces in multiline constructions occupy each its **dedicated line.**

- In **Uniform initialization (Initializer list),** curly braces inherit the coding style from
    **parentheses.** Hence no space between the identifier and `{`, no space after `{` and before
    `}`, and also `}` must be placed at the end of the last line or on a dedicated line.

---------------------------------------------------------------------------------------------------
## Language constructs

### Enumerations

Prefer enum classes, namespace-enclosed enums or Qt enums to C-style enums.

Avoid using enums to declare int constants.

```cpp
//$ Single-line "enum" form, use only if the whole definition fits in one line:
enum class ButtonKind { ok, cancel };

//$ Multiline "enum", prefer it to single-line.
enum class ResultCode
{
    success,
    invalidArguments,
    internalError, //$ Use trailing "," except after a special last item used e.g. for range check.
};

//$ Namespace-enclosed C-style enum - use when implicit conversion to/from int is essential.
//$ A namespace used in place of a class with static-only members inherits class coding style.
namespace ProtocolResponceCode
{
    //$ Enum enclosed in a dedicated namespace is named "Value" and has its items in camelCase.
    enum Value
    {
        ok = 0,
        failure = 113,
        badRequest = 117, //$ Use trailing ",".
    };

    const char* toString(Value value);
    Value fromString(const char* s);
}

//$ Qt flags and Qt enums: use Qt naming convention: enum name as postfix to item names; each word
//$ capitalized.
enum Corner
{
    NoCorners = 0,
    TopLeftCorner = 1 << 0,
    TopRightCorner = 1 << 1,
    BottomLeftCorner = 1 << 2,
    BottomRightCorner = 1 << 3,
    //$ Note omitted trailing "," for the special last item.
    AllCorners = TopLeftCorner | TopRightCorner | BottomLeftCorner | BottomRightCorner
};
Q_DECLARE_FLAGS(Corners, Corner)
```

### Expressions and declarations

```cpp
bool f()
{
    std::function<void()> callback; //$ No space before `()` when omitting function name.
    constexpr int kMaxArgs = 3; //$ Compile-time constants should use constexpr where feasible.

    //$ All local variables that do not change after their definition should be const at all levels).
    //$ NOTE: Function arguments are not required to be const even if they do not change.
    const char* const exeName = argv[0];

    //$ Use octal literals only when needed, and prefix with `/*octal*/`.
    mkdir(dirName, /*octal*/ 0777);

    //$ Use upper-case `L` for suffix. Avoid `int` after `long` and `short`.
    const long longValue = kMaxArgs * 100L;

    //$ Use upper-case `F` for suffix.
    const float floatValue = longValue / 32.0F;

    //$ Always write "int" after "unsigned".
    const unsigned int unsignedValue = 100;

    std::vector<int> v{1, 2, 3}; //$ Uniform initialization (Initializer list): place `{}` as `()`.

    //$ Prefer C-style cast for numeric conversions; do not use constructor-style cast.
    if (v.size() > (size_t) getMaxSize())
        return false;

    //$ Prefer static_cast for non-numeric primitive type conversions; do not use the
    //$ constructor-style cast.
    acceptData(static_cast<const char*>(audioPacket->data()));

    ++p; //$ Prefer the prefix form of `++` and `--`.

    //$ When breaking long lines at binary operators including `.` and `->` (but except `=` and
    //$ `::`), prefer to break before the operator, placing it on the beginning of the next line:
    NX_LOG(debug2, "Removed fixed address for %1: %2")
        .arg(hostName.toString()).arg(hostAddress.toString()));
    const int value =
        veryLongExpression
        + anotherLongExpression;

    //$ When breaking long function calls, prefer breaking after `(`, and if all the arguments do
    //$ not fit into one line, prefer to place each argument on a dedicated line. An exception
    //$ can be made for similar short arguments like `x, y, width, height`, or for arguments that
    //$ are present in most similar calls, like logging tag (which can be left on the call line).
    someLongFunctionName<SpecifyingTheFunctionDoesNotFitInOneLine>(
        shortArg,
        veryLongFunctionArgumentThatPreventsAllArgumentsToFitIntoOneLine);

    someNamespace::SomeLongClassName::
        someLongFunctionName<SpecifyingTheFunctionDoesNotFitInOneLine>(
            authenticationResult, //$ Indent arguments relative to the previous line.
            authInfo);

    //$ For multiline parentheses constructions, place `)` on a new line without an extra indent if
    //$ it cannot be placed at the end of the last line (e.g. because of a trailing comment), or if
    //$ something significant follows it. The same applies to `{...}` when the parentheses style is
    //$ in use (e.g. for initializer lists).
    EXPECT_CALL(
        mockedEmailManager,
        sendAsyncMocked(GMOCK_DYNAMIC_TYPE_MATCHER(const ActivateAccountNotification&))
    ).Times(3);

    //$ Use C++ raw string literals for multiline strings in formal languages like SQL:
    query.prepare(R"sql(
        UPDATE my_table
            SET data = :data
        WHERE id = :id
    )sql").bindValue(":data", data).bindValue(":id", id);

    //$ When a raw string literal is required to have proper indentation and no leading and
    //$ trailing whitespace (e.g. it may be saved to a text file), prefix the literal with
    //$ `/*suppress newline*/ 1 + (const char*)` to skip the initial newline, and use no
    //$ indentation for the content which must start on a new line:
    const std::string fileContent = /*suppress newline*/ 1 + (const char*)
R"json(
{
    id: "0283298a-dda2-45b7-9c1a-f5298fc43683",
    type: "MediaServer",
    endpoints: ["10.0.2.240:7001", "85.93.149.118:7001"]
}
)json"; //$ This should be on a new line to let the final newline appear in the string.

    //$ Operator `,` must always be commented and parenthesized, and used only when necessary:
    const int kSpecial = (sideEffect() /*operator comma*/, mainJob());

    //$ Avoid unnecessary parentheses around operands of not nested `||` and `&&`.
    if (maxCount == 0 || list.count() + 1 < maxCount)
        return list.count() > 0; //$ Do not put parentheses around bool return values.
}

void ternaryOperators()
{
    //$ Single-line form allowed only if the ternary operator and its args fit in one line:
    return data.canConvert<FrameMetadata>() ? data.value<FrameMetadata>() : FrameMetadata();

    //$ Parenthesize args with binary operators except "." and "->".
    return (a < b) ? a : (b + delta);

    //$ Multiline form: if the ternary operator needs line breaking, use only this form:
    return m_baseConnection
        ? m_baseConnection->socket()->getLocalAddress()
        : SocketAddress();
}

//$ If a line break is needed after the function result, indent the next line.
ReallyLongNamespace::ReallyLongResultType
    ReallyLongClassName::reallyLongMethodName(
        //$< Indent function args relative to the line with `(`.
        OtherLongNamespace::ReallyLongArgumentType argument);
```

### Instructions

#### `if`

```cpp
    if (argc > 1)
    {
        parseArg(); //$ Braces required because the `else` part requires them.
    }
    else
    {
        showHelp();
        exit(1);
    }

    if (str == "value1")
        processValue1();
    else if (str == "value2") //$ Write else-if on one line.
        processValue2();
    else
        processOther();

    //$ Avoid `else` after `return` except when both `if` branches are semantically symmetrical
    //$ returns.
    if (!found)
        return false;
    return processFoundItem();

    if (argc == 2)
        return 1; //$ If there are no multiline branches, braces are allowed but not required.
    else //$ Here `else` after `return` is required because branches are semantically symmetrical.
        return 0;

    if (strcmp(str, "pattern") == 0) //$ Use explicit comparison with zero for numeric values.
        useString(str);

    if (pMyObject) //$ Use implicit comparison with null for regular and smart pointers.
        ...;

    //$ For numeric variables defined in `if` use `;`-condition - avoid implicit comparison to 0.
    if (const int r = functionWhichReportsFailureAsZero(); r != 0)
        enjoySuccess();
    else
        reportError();

    if (auto& derived = dynamic_cast<Derived&>(base)) //$ A valid case for var definition in `if`.
        ...;

    if (auto myObject = weakPtrToMyObject.lock()) //$ A valid case for var definition in `if`.
        ...;

    if (performSomeAction(
        someLongArguments)) //$ When `if` condition is multiline, always embrace the branches.
    {
        reportSuccess();
    }

    if (const auto resultCode = performSomeAction(
        someLongArguments) //$ When `if` condition is multiline, place `;` on a dedicated line.
        ;
        resultCode != ResultCode::OK)
    {
        handleError();
    }
```

#### `switch`

```cpp
    switch (value)
    {
        case A: //$ Groupped cases.
        case B:
            f();
            break;
        case C: //$ Case which requires a block.
        {
            Mutex lock;
            criticalAction();
            break;
        }
        //$ Fall-through case: use the C++17 annotation (except grouped cases),
        //$ or `/*fallthrough*/` when C++17 is not available.
        case D1:
            g();
            [[fallthrough]]; //$ Mind the semicolon.
        case D2:
            f();
            return; //$ No "break" after "return".
    }

    //$ Single-line `case` (table-like form of `switch`).
    Value value;
    ...
    switch (value)
    {
        case A: return 'a';
        case B: return 'b';
        case C: return 'c';
    }
```

#### Loops

```cpp
    while (cond) //$ Empty instruction always uses curly braces instead of `;`:
    {
        // Comment an empty block except when absolutely clear why nothing should be done here.
    }

    //$ Infinite loop:
    for (;;)
        ...;

    for (int i = 0; i < count; ++i) //$ Prefer the prefix form of `++` and `--`.
        ...;

    for (auto& item: collection) //$ Prefer for-each loop to iterators and indexes.
        ...;

    //$ Labels:
    //$ Label which marks the end of a code fragment, has an empty line after.
    //$ Label which labels the beginning of a code fragment, has an empty line before.
    //$ Label occupies a dedicated line, and is indented under its block opening curly brace.
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (pixel[x, y] != 0)
                goto breakLoops;
        }
    }
breakLoops:
    //$ Use do-while only when an infinite loop with `break` looks inappropriate.
    do
    {
        doJob();
    } while (condition); //$ `while` is on the same line with `}`; braces are required.
```

### Using \`auto\`

Using `auto` is **required**:

- For a local variable when the type is deduced from a type explicitly **mentioned** in
    the **initializer,** thus, avoiding tautology:
    ```cpp
        const auto connection = dynamic_cast\<ServerConnection\*\>(obtainConnection());
        const auto manager = std::make_unique\<Manager\>(connection);
        const auto context = new Context(manager);
    ```

- For a variable **holding a lambda.**

- Where required by the C++ language.

Using `auto` is **allowed**:

- For a **local variable** when the type does **not need to be known** to a reader to understand
    its usage, and the code is **ready** for potential **type change:**
    ```
        const auto handler = getHandler();
        delegate.setHandler(handler); //$ We don't care what the `handler` actually is.
    ```

- For a **local variable of a non-primitive type,** including iterating via iterators or
    `for(...:...)`, when specifying the type would **not help** a reader to **understand** the
    variable **usages.**

- For a non-primitive return type of a .cpp-local function in case the last statement in the
    function is `return`, and the type can be clearly deduced from the expression.

Besides these cases, `auto` should not be used. A **long or complex type** name is by itself
**not** a valid reason for using `auto`.

### Lambdas

```cpp
    //$ Single-line lambda, allowed if its whole expression fits in one line:
    return any_of(range, [value](const ApiData& data) { return data.id == value; });

    //$ Single-line lambda, allowed if its whole function argument fits in one line:
    return any_of(
        range,
        [value](const ApiData& data) { return data.id == value; },
        someOtherArgument);

    //$ Multiline lambda in local const or var initialization.
    const auto handler =
        [this, peerData, connection]() //$ Lambda's `[` is indented.
        {
            QnMutexLocker lock(&m_mutex);
            const auto peerIter = m_peers.find(peerData);
            if (peerIter == m_peers.end())
                return;
        };

    //$ Multiline lambda in any expression.
    return any_of(
        range,
        [value](const ApiData& data) //$ Lambda's `[` is indented.
        {
            return data.id == value;
        },
        someOtherArgument);

    //$ Lambda with multiline `[]`: `{` is under `[`, args are indented from the preceding line.
    m_authenticationManager->authenticate(
        *this,
        request,
        [this, weakThis = std::move(weakThis),
            requestMessage = std::move(requestMessage)](
                bool authenticationResult,
                stree::ResourceContainer authInfo)
        {
            return doJob(weakThis, requestMessage);
        });

    //$ Lambda with explicit return type.
    auto handler = []() -> ResultCode { return ResultCode::ok; }; //$ Surround return-type `->` with spaces.
```

### Macros

```cpp
//$ Prefer functions and templates to macros.
//$ Macros, including defined in cpp files, should be prefixed with `NX_` to avoid conflicts.
//$ Where possible, prefer delegating macro job to a function.

/** Single-line macro. */
#define NX_LOG_VALUE(VALUE) log((VALUE), #VALUE, __FILE__, __LINE__)

/**
 * Multiline macro, can be used as a single instruction: indent regularly, space before `\`.
 * @param COND Condition to activate logging.
 * @param EXPR Is not evaluated if COND fails.
 */
#define NX_LOG_IF(COND, EXPR) do \
{ \
    /* Comment in a macro: note that `//` comments are not compatible with `\`. */ \
    if (COND) \
        NX_LOG_VALUE(EXPR); \ //$ Extra parentheses around ARG here are not needed.
} while (0)
```

### Classes

```cpp
//$ Use `struct` for and only for POD-like types, without `private` and `protected` sections.
struct PlainOldData:
    PodBase //$ Inherit `struct` only from `struct` (omit `public`), `class` only from `class`.
{
    int number; //$ Place POD fields before methods.

    PlainOldData(int number): number(number) {} //$ POD can have simple constructor(s).

    QString toString(); //$ POD can have methods, e.g. for conversion, debugging and logging.
};

struct //$ Unnamed `struct`.
{
    int x;
    int y;
} point;

class MyObject: public MyBase //$ Single-line form for inheritance, allowed only for single base.
{
    using base_type = MyBase; //$ Synonym for the base class type; define only if needed.

public:
    //$ Single-line in-class inline function definition. Omit `inline` keyword.
    MyObject(int value): base_type(value) {}

    virtual ~MyObject() override;

    //$ Multiline in-class inline function definition. Omit `inline` keyword.
    virtual void myFunction() override
    {
        ...
        base_type::myFunction();
        ...
    }

    ...
};

class ListeningPeerPool: //$ Multiline form for inheritance; each base on a dedicated line.
    public QObject,
    public Singleton<ListeningPeerPool>, //< Needed for unit tests only.
    private PrivateBase
{
    Q_OBJECT

public: //$ Empty line before each section, except right after `{`.
    virtual ~ListeningPeerPool() = default; //$ Prefer `default` virtual destructor to `{}`.

    virtual void doSomething() = 0;
    virtual void someOtherMethod() override;

    void doNothing() const //$ Multiline form of an empty inline method in class definition.
    {
    }

    int setField(int value) //$ Multiline form of an inline method in class definition.
    {
        m_field = value;
    }

    //$ Single-line form allowed if the entire inline method definition fits in one line.
    ListeningPeerPool(int someField): m_someField(someField) {}
    int setSomeField(int someField) { m_someField = someField; }
    void doNothing2() const {}

    //$ Separate multiline declaration from neighboring declarations with empty lines.
    //$ For line breaking, use the same rules as for long function calls.
    void veryLongFunctionDeclaration(
        int firstParameter,
        int secondParameter);

private: //$ Types have a dedicated section.
    typedef void (*Callback)(int arg); //$< Prefer `typedef` for function pointers.

private: //$ Fields have a dedicated section.
    static QMutex s_mutex; //$ Static fields are prefixed with `s_`.
    int m_someField; //$ Non-static private fields are prefixed with `m_`.

private: //$ Functions have a dedicated section.
    void internalMethod();
};

/**
 * Interfaces that are ABI-agnostic (in particular, do not use STL types in methods) must use the
 * `I` prefix. Such interfaces must be header-only, and besides pure virtual functions may include
 * inline methods like wrappers for virtual methods.
 */
class IString
{
    virtual ~IString() = default;
    virtual std::string value() = 0;
};
```

### Member initializer lists in constructors

```cpp
//$ Single-line form, allowed only if the constructor definition before `{` fits in one line.
MyData::MyData(int number): m_number(number)
{
    ...
}

//$ Break the line after `:` if the constructor declaration before `:` fits in one line.
AsyncClientUser::AsyncClientUser(std::shared_ptr<AsyncClient> client):
    m_operationsInProgress(0),
    m_client(client) //$ Prefer each field initializer on a dedicated line.
{
}

//$ Multiline form, used if and only if constructor args are multiline.
AsyncClientUser::AsyncClientUser(
    std::shared_ptr<AsyncClient> client,
    int anotherArgument,
    char theLastArgument)
    :
    m_operationsInProgress(0),
    m_client(client)
{
}

struct PlainOldData
{
    int i;

    //$ Use the same name for parameters and fields in POD initialization list.
    PlainOldData(int i): i(i)
    {
        //$ Use `this->` when absolutely necessary to refer to a field rather than a constructor
        //$ parameter with the same name.
        ++(this->i);
    }
};
```

### Templates

```cpp
/**
 * Class template description.
 */
template<typename T, class C = std::list<int>> //$ `class` on a dedicated line. No space in `>>`.
class MyClass
{
};

/**
 * Function template.
 */
template<typename T> //$ `template` on a dedicated line.
void doJob(T arg)
{
}

/**
 * Template with multiline args - always use when `template<...>` does not fit in one line.
 */
template<
    typename LongType,
    typename = typename std::enable_if<std::is_integral<LongType>::value>::type
>
struct FakeTcpTunnelAcceptor
{
};

/**
 * Class template with template arguments that require synonyms defined in the class:
 */
template<typename Request_> //$ Use `_` suffix to differ from in-class definitions.
class Executor
{
public:
    using Request = Request_;
};

/**
 * Full template specialization, multiline form.
 */
template<>
uint64_t parseHumanReadableString<uint64_t>(const QString& humanReadableString, bool* outSuccess);

/** Full template specialization, single-line form - no newline after `template<>`. */
template<> Manager* Singleton<Manager>::s_instance;

/** Use template argument comments when their purpose is unclear. */
using FreeSpaceByStorage = std::map<std::string /*storageId*/, int /*byteCount*/>;
```
