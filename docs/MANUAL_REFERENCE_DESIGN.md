# CXXMAN: Manual Reference System

## Design Specification & Implementation Plan

**Project:** cxxman
**Scope:** C++ Standard Library only
**Implementation:** Python
**Status:** Initial read-time projection implemented; full design remains a direction
**Primary Objective:** Transform verbose cppreference-derived documentation into concise, structured, Unix-style manual pages.

## Implementation status

The current source projects cached generic documents into dynamic manual
sections at read time. The catalog contains 50 curated pages, and saved
fixtures exercise six representative pages. Rich renders C++ code with
syntax highlighting and supports `--color auto|always|never`. The design
below remains broader than the implementation: it does not promise
complete C++ library coverage or structured extraction of every fact from
every cppreference page. Unknown or ambiguous source content remains in
the stored generic document.

---

# 1. Context

CXXMAN is an existing cross-platform CLI documentation tool that retrieves and presents C++ documentation derived from cppreference.

The current implementation already has its own parsing, document representation, storage, caching, searching, and terminal rendering mechanisms.

When this proposal was written, the output still resembled the original
cppreference article structure. The implementation status above describes
the current source tree.

This creates a usability problem: users frequently need only a small amount of information, but must navigate extensive documentation to find it.

CXXMAN should evolve from an article-oriented documentation viewer into a manual-reference system.

**This is an enhancement to the existing project, not a request to rewrite it.**

# 2. Vision

The goal is to provide a concise C++ reference directly in the terminal, following the information architecture of traditional Unix manual pages.

A user should be able to execute:

```bash
cxxman vector
```

And quickly understand:

- What is `std::vector`?
- Which header provides it?
- What is its declaration?
- What are its template parameters?
- What operations are available?
- What do those operations return?
- What are their complexity guarantees?
- What important invalidation rules apply?
- Where can more detailed information be found?

The default output should provide enough technical information to start using an entity without reading an entire cppreference article.

For exhaustive explanations, users can still consult cppreference.

# 3. Core Design Principles

## 3.1 Concise, Not Incomplete

The objective is not simply to shorten documentation.

It is to prioritize information based on practical usage.

Preserve important technical guarantees, requirements, return behavior, exceptions, complexity, and portability information.

Remove unnecessary explanations, duplicated content, extensive examples, navigation elements, and unrelated article sections.

Do not sacrifice correctness for brevity.

## 3.2 Entity-Oriented Documentation

Different C++ entities require different documentation structures.

CXXMAN must recognize that these are fundamentally different:

- Namespace
- Header
- Class
- Struct
- Function
- Member function
- Template
- Type alias
- Concept
- Enumeration
- Macro
- Operator
- Algorithm

A universal fixed page template is inappropriate.

## 3.3 Dynamic Sections

Every page should contain only applicable sections.

For example:

A container may have:

- SYNOPSIS
- TEMPLATE PARAMETERS
- MEMBER FUNCTIONS
- ITERATORS
- COMPLEXITY
- INVALIDATION

An algorithm may have:

- SYNOPSIS
- PARAMETERS
- RETURN VALUE
- REQUIREMENTS
- COMPLEXITY
- EXCEPTIONS

A type alias may have:

- SYNOPSIS
- DESCRIPTION
- TYPE INFORMATION
- SEE ALSO

The renderer must not create empty sections.

## 3.4 Source-Backed Information

All displayed technical information must originate from extracted documentation or verified metadata.

Never invent:

- Return types
- Function signatures
- Complexity guarantees
- Exception guarantees
- Standard availability
- Thread-safety guarantees
- Preconditions
- Platform compatibility

When information is unavailable, omit it or explicitly mark it as unavailable.

## 3.5 Preserve Existing Architecture

Before implementation, inspect the complete existing repository.

Understand the current:

- HTML parser
- Document model
- Data extraction
- Storage layer
- Cache mechanism
- Search implementation
- Terminal renderer
- Tests

Reuse existing abstractions whenever appropriate.

Do not introduce unnecessary dependencies or duplicate existing functionality.

---

# 4. Target Documentation Format

The visual presentation should follow traditional Unix manual pages.

Example:

```text
VECTOR(3)                 C++ Library Manual                 VECTOR(3)

NAME
    vector - dynamic sequence container

LIBRARY
    C++ Standard Library

SYNOPSIS
    #include <vector>

    namespace std {
        template<class T, class Allocator = allocator<T>>
        class vector;
    }

DESCRIPTION
    Sequence container that stores elements contiguously.

TEMPLATE PARAMETERS
    T
        Element type.

    Allocator
        Allocator used for memory management.

MEMBER FUNCTIONS
    push_back()
        Append an element.

    pop_back()
        Remove the last element.

    size()
        Return the number of elements.

    empty()
        Check whether the container is empty.

COMPLEXITY
    size()
        Constant complexity.

SEE ALSO
    array(3), deque(3), list(3)
```

This is a presentation example. Actual declarations, descriptions, and guarantees must be obtained from the source documentation.

The format should remain compact, readable, and consistent.

# 5. Proposed Documentation Sections

Sections should be modular and optional.

## 5.1 General Sections

| Section     | Purpose                           |
| ----------- | --------------------------------- |
| NAME        | Entity name and short description |
| LIBRARY     | C++ Standard Library              |
| SYNOPSIS    | Header, namespace, declarations   |
| DESCRIPTION | Concise explanation               |
| STANDARDS   | C++ standard availability         |
| SEE ALSO    | Related entities                  |
| REFERENCES  | Original documentation source     |

## 5.2 Function Sections

| Section      | Purpose                              |
| ------------ | ------------------------------------ |
| SIGNATURE    | Function declaration                 |
| PARAMETERS   | Parameter names, types, descriptions |
| RETURN VALUE | Return type and behavior             |
| COMPLEXITY   | Complexity guarantees                |
| REQUIREMENTS | Constraints and preconditions        |
| EXCEPTIONS   | Exception behavior                   |
| NOTES        | Important details                    |
| EXAMPLES     | Minimal usage                        |

## 5.3 Class and Container Sections

| Section             | Purpose                         |
| ------------------- | ------------------------------- |
| TEMPLATE PARAMETERS | Template parameter information  |
| MEMBER TYPES        | Public nested types             |
| MEMBER FUNCTIONS    | Available operations            |
| CONSTRUCTORS        | Construction operations         |
| ITERATORS           | Iterator interfaces             |
| CAPACITY            | Size and capacity operations    |
| MODIFIERS           | Element modification operations |
| ELEMENT ACCESS      | Element access operations       |
| INVALIDATION        | Iterator/reference invalidation |
| COMPLEXITY          | Operation complexity            |

## 5.4 Other Potential Sections

- TYPE ALIASES
- ENUMERATIONS
- OPERATORS
- CONSTANTS
- CONCEPTS
- THREAD SAFETY
- DEPRECATED
- FEATURE TEST MACROS

Not every section needs to be implemented immediately.

The system should be extensible.

---

# 6. Proposed Architecture

The existing architecture should be inspected before deciding the exact integration points.

The conceptual target pipeline is:

```text
                 cppreference
                      |
                      v
                 HTML Parser
                      |
                      v
              Existing Document Model
                      |
                      v
            Semantic Extraction Layer
                      |
                      v
              Manual Page Projection
                      |
                      v
              Semantic Manual Model
                      |
                      v
               Terminal Renderer
                      |
                      v
                CXXMAN Output
```

The new functionality should sit between the existing document representation and terminal rendering, unless repository inspection reveals a more appropriate integration point.

## 6.1 Existing Document Model

Continue using the existing generic document representation as the source of extracted content.

It represents the parsed article structure.

Do not unnecessarily replace it with a manual-specific representation.

## 6.2 Semantic Extraction Layer

Introduce a layer responsible for identifying useful information from the generic document.

Its responsibilities may include:

- Identifying the entity type.
- Identifying the primary header.
- Extracting declarations.
- Identifying function signatures.
- Extracting parameter descriptions.
- Extracting return information.
- Extracting complexity guarantees.
- Extracting relevant member operations.
- Identifying standard-version information.
- Filtering unrelated article content.

This layer must work with the actual structures produced by the existing parser.

Do not assume cppreference uses identical HTML structures for every entity.

## 6.3 Manual Page Projection

Convert extracted information into a concise manual-page representation.

This is a semantic transformation, not simply HTML formatting.

The projection should:

1. Select relevant information.
2. Group information into logical sections.
3. Normalize section content.
4. Remove irrelevant or duplicated information.
5. Preserve source attribution.
6. Preserve ordering where meaningful.
7. Avoid generating unsupported content.

## 6.4 Semantic Manual Model

Introduce a structured representation for the final manual page.

Conceptual example:

```python
ManualPage(
    name="vector",
    entity_type="class",
    language="cpp",
    header="vector",
    sections=[
        NameSection(...),
        LibrarySection(...),
        SynopsisSection(...),
        DescriptionSection(...),
        TemplateParametersSection(...),
        MemberFunctionsSection(...),
        ComplexitySection(...),
        SeeAlsoSection(...),
    ],
)
```

This is illustrative pseudocode, not a requirement to use these exact class names.

The actual implementation should follow existing project conventions.

The important requirement is that sections are structured, typed, and optional.

---

# 7. Dynamic Section System

This is a central architectural requirement.

Do not implement one large renderer containing hardcoded assumptions about every entity.

Prefer a modular section registry or equivalent extensible dispatch mechanism.

Conceptual example:

```python
SECTION_RENDERERS = {
    "name": render_name,
    "library": render_library,
    "synopsis": render_synopsis,
    "description": render_description,
    "parameters": render_parameters,
    "return_value": render_return_value,
    "complexity": render_complexity,
    "exceptions": render_exceptions,
    "examples": render_examples,
    "see_also": render_see_also,
}
```

Rendering should depend on the actual sections available in a page.

```python
def render_page(page):
    render_header(page)

    for section in page.sections:
        renderer = SECTION_RENDERERS.get(section.type)

        if renderer is not None:
            renderer(section)
```

The final implementation should provide appropriate validation and error handling.

Unknown section types should not crash the entire application.

The system must support adding new section types without requiring changes to unrelated entity definitions.

---

# 8. Static Data vs Dynamic Rendering

Documentation content should remain statically stored or cached.

The renderer should be dynamic.

These are separate concerns.

**Static documentation data:**

- Entity names
- Signatures
- Descriptions
- Parameters
- Return information
- Complexity
- Exceptions
- Standard versions

**Dynamic rendering:**

- Which sections exist
- Section ordering
- Terminal formatting
- Compact versus detailed presentation
- Optional filtering

This approach avoids generating every page from hardcoded templates.

It also avoids requiring an LLM or runtime network access to construct documentation.

The semantic extraction process should be deterministic and reproducible.

---

# 9. Information Extraction and Reliability

The transformation must be conservative.

Cppreference contains many different article structures, overloads, specializations, and nested sections.

A generic HTML-to-text conversion is insufficient for reliable manual-page generation.

The extraction layer should distinguish:

1. Information explicitly present in the source.
2. Information that can be safely normalized.
3. Information that cannot be reliably extracted.

Only the first two categories should appear as factual output.

For example, do not infer that a function is thread-safe merely because its documentation does not mention thread safety.

Do not infer complexity from implementation intuition.

Do not merge different overloads into one declaration if their signatures differ.

Preserve overload distinctions when they materially affect usage.

If extraction confidence is insufficient, retain the information in the detailed source view rather than generating an inaccurate summary.

---

# 10. Detail Mode

The project should eventually support two presentation levels.

## Default Mode

Concise manual page.

```bash
cxxman vector
```

Shows essential information.

## Detailed Mode

Displays more extracted documentation.

```bash
cxxman vector --detail
```

This mode can preserve additional descriptions, overload details, requirements, examples, and technical notes.

The existing article-oriented representation may be useful for this mode.

Do not discard information merely because it is not needed in the default view.

The detailed mode is a future capability unless existing functionality already supports it.

---

# 11. C++ Standard Version Awareness

CXXMAN focuses exclusively on C++.

Documentation should distinguish language-standard availability.

Relevant versions include:

- C++11
- C++14
- C++17
- C++20
- C++23
- Later standards as supported by the source dataset

Where possible, identify:

- Introduced version
- Modified version
- Deprecated version
- Removed version
- Feature-test macro

Do not assume an entity is available in every C++ version.

Potential future CLI usage:

```bash
cxxman vector --std c++17
```

This is an optional filtering feature, not a prerequisite for the initial refactor.

---

# 12. Implementation Phases

## Phase 0: Repository Audit

Before writing code:

- Inspect the complete repository structure.
- Read the existing document model.
- Read the HTML parser.
- Read the renderer.
- Inspect storage and cache behavior.
- Inspect representative cached documentation.
- Inspect current tests.
- Run the existing test suite.

Identify the smallest architectural change that achieves the target.

Deliverable: a short implementation plan referencing actual repository files.

## Phase 1: Semantic Model

Design the manual-page representation.

Requirements:

- Optional sections.
- Typed section data.
- Entity metadata.
- Source attribution.
- Extensibility.
- Serialization compatibility where necessary.

Avoid implementing every possible section at once.

## Phase 2: Semantic Extraction

Implement extraction for a small set of representative C++ entities.

Recommended initial entities:

1. `std::vector`
2. `std::string`
3. `std::sort`
4. `std::unique`
5. `std::optional`

These represent different documentation structures.

Test extraction quality before expanding coverage.

## Phase 3: Manual Renderer

Implement the Unix-style terminal layout.

Requirements:

- Consistent headings.
- Readable indentation.
- Proper code formatting.
- Long-line handling.
- Terminal-width awareness.
- Optional sections.
- Graceful handling of missing information.

Reuse the existing terminal formatting library if appropriate.

## Phase 4: Integration

Integrate the new manual-page output into the existing CLI.

Preserve:

- Existing search functionality.
- Existing caching.
- Existing offline access.
- Existing update mechanisms.
- Existing cross-platform behavior.

Avoid breaking existing commands unless a change is explicitly justified.

## Phase 5: Validation

Compare output against original cppreference documentation.

Validate:

- Signatures.
- Return types.
- Parameter descriptions.
- Overloads.
- Complexity.
- Exceptions.
- Standard availability.
- Related entities.

Expand coverage only after the extraction process proves reliable.

---

# 13. Testing Requirements

Tests should cover both individual components and complete documentation pages.

## Unit Tests

- Section creation.
- Section validation.
- Semantic extraction.
- Entity classification.
- Signature extraction.
- Missing information.
- Unknown section types.
- Renderer dispatch.
- Serialization.

## Integration Tests

Use real or sanitized cached cppreference pages.

At minimum:

- A class page.
- A container page.
- An algorithm page.
- A function page.
- A type page.

## Regression Tests

Existing functionality must remain operational.

Verify:

- Search.
- Cache loading.
- Offline access.
- Updates.
- Existing CLI commands.
- Cross-platform rendering.

## Accuracy Tests

Do not test only whether output contains text.

Test whether the extracted technical information is correct.

For example, verify that overloads are not accidentally merged and that return types remain accurate.

---

# 14. Non-Goals

This enhancement does not aim to:

- Rewrite the HTML parser.
- Replace the storage engine.
- Replace the existing cache.
- Replace the search engine.
- Replace cppreference.
- Reimplement the C++ standard library.
- Generate explanations using an LLM.
- Introduce a mandatory section template.
- Support C documentation at this stage.
- Add POSIX or operating-system-specific APIs to the primary scope.

---

# 15. Acceptance Criteria

The enhancement is considered successful when:

1. CXXMAN displays concise Unix-style manual pages.
2. Different C++ entity types can have different section structures.
3. Empty sections are never rendered.
4. Function signatures and return information remain accurate.
5. Important technical guarantees are preserved.
6. Information is not fabricated when extraction fails.
7. The original article data remains available for detailed reference.
8. Existing search and offline functionality continue working.
9. The new architecture is extensible.
10. The existing project does not require an unnecessary rewrite.

---

# 16. Instructions to the Coding Agent

Before implementation:

1. Read the repository, not just this document.
2. Identify the actual existing architecture.
3. Map the proposed components to existing files.
4. Identify reusable code.
5. Identify architectural risks.
6. Present a concise implementation plan.

During implementation:

- Make incremental changes.
- Preserve existing behavior.
- Avoid unnecessary abstractions.
- Avoid introducing dependencies without justification.
- Follow existing naming and coding conventions.
- Add tests alongside implementation.
- Do not silently discard existing extracted content.

Do not implement all phases in one large change.

Start with Phase 0, then Phase 1.

After each phase, run relevant tests and report:

- Files changed.
- Architectural decisions.
- Tests executed.
- Known limitations.
- Remaining work.

**Important:** The purpose is to improve information architecture and presentation, not to replace a working project with a theoretically cleaner but unnecessarily complicated implementation.

---

# Final Design Principle

CXXMAN should behave like a manual reference, not an article reader.

The user should be able to answer practical programming questions quickly:

- What is this?
- Where is it defined?
- What is its declaration?
- How do I use it?
- What does it return?
- What guarantees does it provide?
- What should I watch out for?

Everything else belongs in the detailed reference.
