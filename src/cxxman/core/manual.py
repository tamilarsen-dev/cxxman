"""Project parsed reference documents into concise, source-backed cards."""

from __future__ import annotations

from dataclasses import dataclass, replace
import re
import textwrap

from cxxman.core.catalog import SOURCE_PAGES
from cxxman.core.document import (
    CodeBlock,
    ContentBlock,
    Document,
    Heading,
    ListBlock,
    Paragraph,
    Reference,
    Table,
)


@dataclass(frozen=True, slots=True)
class ManualSection:
    """A non-empty card section and an optional hidden-content hint."""

    title: str
    blocks: tuple[ContentBlock, ...]
    hint: str | None = None


@dataclass(frozen=True, slots=True)
class ManualPage:
    """A concise view of a generic parsed document."""

    name: str
    entity_kind: str
    header: str | None
    source_url: str
    sections: tuple[ManualSection, ...]
    topic: str = ""
    purpose: str | None = None


_SECTION_TITLES = {
    "description": "DESCRIPTION",
    "overview": "DESCRIPTION",
    "member functions": "MEMBER FUNCTIONS",
    "observers": "OBSERVERS",
    "member types": "MEMBER TYPES",
    "nested types": "MEMBER TYPES",
    "template parameters": "TEMPLATE PARAMETERS",
    "parameters": "PARAMETERS",
    "return value": "RETURNS",
    "returns": "RETURNS",
    "complexity": "COMPLEXITY",
    "exceptions": "EXCEPTIONS",
    "standard": "STANDARDS",
    "standards": "STANDARDS",
    "standard versions": "STANDARDS",
    "requirements": "REQUIREMENTS",
    "restrictions": "REQUIREMENTS",
    "type requirements": "REQUIREMENTS",
    "invalidations": "INVALIDATION",
    "invalidation": "INVALIDATION",
    "iterator invalidation": "INVALIDATION",
    "iterators": "ITERATORS",
    "capacity": "CAPACITY",
    "element access": "ELEMENT ACCESS",
    "modifiers": "MODIFIERS",
    "notes": "NOTES",
    "example": "EXAMPLE",
    "examples": "EXAMPLE",
    "see also": "SEE ALSO",
    "references": "SEE ALSO",
    "synopsis": "SYNOPSIS",
    "declarations": "SYNOPSIS",
    "non-member functions": "NON-MEMBERS",
    "non-member function": "NON-MEMBERS",
    "specializations": "SPECIALIZATIONS",
    "helper classes": "HELPER CLASSES",
    "helper specializations": "HELPER CLASSES",
    "helpers": "HELPERS",
    "deduction guides": "DEDUCTION GUIDES",
    "member constants": "VALUE",
    "value": "VALUE",
    "data members": "MEMBER TYPES",
}

_EXCLUDED_HEADINGS = {
    "compiler support",
    "defect reports",
    "possible implementation",
    "feature test macros",
}
_FUNCTION_ORDER = (
    "SYNOPSIS",
    "PARAMETERS",
    "RETURNS",
    "COMPLEXITY",
    "WATCH OUT",
    "EXAMPLE",
    "SEE ALSO",
)
_CLASS_ORDER = (
    "SYNOPSIS",
    "DESCRIPTION",
    "MEMBERS",
    "NON-MEMBERS",
    "WATCH OUT",
    "COMPLEXITY",
    "EXAMPLE",
    "SEE ALSO",
)
_OTHER_ORDER = (
    "SYNOPSIS",
    "DESCRIPTION",
    "TEMPLATE PARAMETERS",
    "VALUE",
    "EXAMPLE",
    "SEE ALSO",
)
_IMPORTANT_TERMS = (
    "undefined",
    "ill-formed",
    "invalidated",
    "invalidates",
    "invalidated",
    "precondition",
    "not guaranteed",
    "no effect",
    "unspecified",
    "throws",
    "thrown",
)
_IMPLEMENTATION_NAMES = ("libstdc++", "libc++", "MSVC", "Microsoft Visual C++")
_ENTITY_KIND_OVERRIDES = {
    "all_any_none_of": "algorithm",
    "cout": "object",
    "header_algorithm": "header",
    "is_same": "type trait",
    "same_as": "concept",
}


def project_document(document: Document) -> ManualPage:
    """Select short, exact source passages for the default terminal card.

    The input document remains the complete parsed source representation.
    Content omitted here is available through the page's full/source hint.
    """
    entity_kind = _classify_entity(document.canonical_topic)
    before_sections, source_sections = _split_sections(document)
    paragraphs = [block for block in before_sections if isinstance(block, Paragraph)]
    purpose_block = next((block for block in paragraphs if block.text.strip()), None)
    purpose = (
        _first_sentence(purpose_block.text)
        if purpose_block
        else (document.purpose.strip() if document.purpose else None)
    )

    declarations = [
        block
        for block in before_sections
        if isinstance(block, CodeBlock) and _looks_like_declaration(block)
    ]
    if not declarations:
        declarations = _blocks(source_sections.get("SYNOPSIS", ()), CodeBlock)

    sections: dict[str, ManualSection] = {}
    synopsis, synopsis_hint = _select_declarations(declarations, entity_kind, document.canonical_topic)
    _put(sections, "SYNOPSIS", synopsis, synopsis_hint)

    if entity_kind not in {"algorithm", "member function", "class"}:
        template_parameters, template_hint = _template_parameters(
            source_sections.get("TEMPLATE PARAMETERS", ()), document.canonical_topic
        )
        _put(sections, "TEMPLATE PARAMETERS", template_parameters, template_hint)
        value_blocks, value_hint = _short_prose_section(
            source_sections.get("VALUE", ()), document.canonical_topic, "value details"
        )
        _put(sections, "VALUE", value_blocks, value_hint)

    extra_blocks = [block for block in before_sections if block is not purpose_block]
    implementation_notes, extra_blocks = _take_implementation_notes(extra_blocks)
    extra_paragraphs = [block for block in extra_blocks if isinstance(block, Paragraph)]
    if implementation_notes:
        sections["NOTES"] = ManualSection("NOTES", tuple(implementation_notes))

    if entity_kind != "algorithm":
        description = _first_description(extra_paragraphs)
        if description is None:
            description = _first_description(source_sections.get("DESCRIPTION", []))
        if description:
            _put(sections, "DESCRIPTION", (description,))

    parameter_blocks, parameter_hint = _parameters(
        source_sections.get("PARAMETERS", ()), document.canonical_topic, declarations
    )
    _put(sections, "PARAMETERS", parameter_blocks, parameter_hint)

    return_blocks, return_hint = _short_prose_section(
        source_sections.get("RETURNS", ()), document.canonical_topic, "return-value details"
    )
    _put(sections, "RETURNS", return_blocks, return_hint)

    complexity_source = list(source_sections.get("COMPLEXITY", ()))
    if not complexity_source:
        complexity_source = _intro_complexity(before_sections)
    complexity_blocks, complexity_hint = _short_prose_section(
        complexity_source, document.canonical_topic, "complexity details"
    )
    _put(sections, "COMPLEXITY", complexity_blocks, complexity_hint)

    hidden_overload_labels = {
        block.label
        for block in declarations
        if "ExecutionPolicy" in block.code and block.label
    }
    pitfalls, pitfalls_hint = _watch_out(
        before_sections,
        source_sections,
        document.canonical_topic,
        hidden_overload_labels,
    )
    _put(sections, "WATCH OUT", pitfalls, pitfalls_hint)

    if entity_kind in {"class", "unknown"}:
        members, member_hint = _member_names(
            source_sections.get("MEMBER FUNCTIONS", ()), document.canonical_topic
        )
        _put(sections, "MEMBERS", members, member_hint)
        non_members, non_member_hint = _member_names(
            source_sections.get("NON-MEMBERS", ()), document.canonical_topic
        )
        _put(sections, "NON-MEMBERS", non_members, non_member_hint)

    example_blocks, example_hint = _example(
        source_sections.get("EXAMPLE", ()), document.canonical_topic
    )
    _put(sections, "EXAMPLE", example_blocks, example_hint)

    prelude_references = [block for block in before_sections if isinstance(block, Reference)]
    see_also, see_also_hint = _see_also(
        [*prelude_references, *source_sections.get("SEE ALSO", ())],
        document.canonical_topic,
    )
    _put(sections, "SEE ALSO", see_also, see_also_hint)

    # Preserve a named source standard section when the parser found one.
    standards = _first_source_paragraph(source_sections.get("STANDARDS", ()))
    _put(sections, "STANDARDS", (standards,) if standards else ())

    if entity_kind == "unknown":
        sections = {
            name: section
            for name, section in sections.items()
            if name in {"SYNOPSIS", "DESCRIPTION"}
        }
    order = (
        _FUNCTION_ORDER
        if entity_kind in {"algorithm", "member function"}
        else (_CLASS_ORDER if entity_kind == "class" else _OTHER_ORDER)
    )
    ordered_sections = tuple(sections[name] for name in order if name in sections)
    ordered_sections += tuple(
        section
        for name, section in sections.items()
        if name not in order and name != "NOTES"
    )
    if "NOTES" in sections:
        ordered_sections += (sections["NOTES"],)

    page = ManualPage(
        name=document.title,
        entity_kind=entity_kind,
        header=document.header,
        source_url=document.source_url,
        sections=ordered_sections,
        topic=document.canonical_topic,
        purpose=purpose,
    )
    return _fit_quick_card(page)


def manual_page_line_count(page: ManualPage, width: int = 100) -> int:
    """Estimate rendered card lines using the same layout rules as Rich output."""
    safe_width = max(20, width)
    title = page.name + (" — " + page.purpose if page.purpose else "")
    lines = 3 + _wrapped_line_count(title, safe_width)
    if page.header:
        lines += 1
    for section in page.sections:
        lines += 2 + _wrapped_line_count(section.title, safe_width)
        for block in section.blocks:
            if isinstance(block, Paragraph):
                lines += _wrapped_line_count(block.text, safe_width - 4) + 1
            elif isinstance(block, CodeBlock):
                lines += sum(
                    _wrapped_line_count(line, safe_width - 4)
                    for line in block.code.splitlines()
                ) + 1
            elif isinstance(block, ListBlock):
                lines += sum(
                    _wrapped_line_count(item, safe_width - 4) + 1
                    for item in block.items
                )
            elif isinstance(block, Table):
                lines += 2 + sum(
                    max(
                        (
                            _wrapped_line_count(cell, max(12, safe_width // max(1, len(row))))
                            for cell in row
                        ),
                        default=1,
                    )
                    for row in block.rows
                )
            else:
                lines += 2
        if section.hint:
            lines += _wrapped_line_count(section.hint, safe_width)
    if page.source_url:
        footer = f"More: cxxman {page.topic or page.name} --full · {page.source_url}"
        lines += _wrapped_line_count(footer, safe_width)
    return lines


def _fit_quick_card(page: ManualPage) -> ManualPage:
    budgets = {"algorithm": 30, "member function": 30, "class": 42}
    budget = budgets.get(page.entity_kind)
    if budget is None or manual_page_line_count(page) <= budget:
        return page

    sections = list(page.sections)

    def current_page() -> ManualPage:
        return replace(page, sections=tuple(sections))

    def append_hint(title: str, message: str) -> None:
        section_index = next(
            (index for index, section in enumerate(sections) if section.title == title),
            None,
        )
        if section_index is None:
            section_index = next(
                (index for index, section in enumerate(sections) if section.title == "SYNOPSIS"),
                None,
            )
        if section_index is None:
            sections.append(ManualSection("MORE", (), message))
            return
        section = sections[section_index]
        hints = [hint for hint in (section.hint, message) if hint]
        sections[section_index] = replace(section, hint="; ".join(dict.fromkeys(hints)))

    def hide_section(title: str, message: str) -> None:
        section_index = next(
            (index for index, section in enumerate(sections) if section.title == title),
            None,
        )
        if section_index is None:
            return
        section = sections.pop(section_index)
        hint = section.hint
        existing_hint_covers_message = bool(hint) and (
            (title == "EXAMPLE" and "example" in hint.casefold())
            or (title == "SEE ALSO" and "related name" in hint.casefold())
        )
        if not existing_hint_covers_message:
            append_hint("SYNOPSIS", message)
        if hint:
            append_hint("SYNOPSIS", hint)

    def limit_section(title: str, limit: int, label: str) -> None:
        section_index = next(
            (index for index, section in enumerate(sections) if section.title == title),
            None,
        )
        if section_index is None:
            return
        section = sections[section_index]
        if len(section.blocks) <= limit:
            return
        hidden = len(section.blocks) - limit
        message = f"+{hidden} {label} block(s) — cxxman {page.topic} --full"
        hints = [hint for hint in (section.hint, message) if hint]
        selected_blocks = section.blocks[:limit]
        if title == "MEMBERS":
            priorities = {
                "element access": 0,
                "modifiers": 1,
                "capacity": 2,
                "operations": 3,
                "observers": 4,
                "iterators": 5,
            }
            ranked_blocks = sorted(
                enumerate(section.blocks),
                key=lambda item: (
                    priorities.get(
                        item[1].text.split(":", 1)[0].strip().casefold(),
                        len(priorities),
                    ),
                    item[0],
                ),
            )[:limit]
            selected_indices = {index for index, _ in ranked_blocks}
            selected_blocks = tuple(
                block
                for index, block in enumerate(section.blocks)
                if index in selected_indices
            )
        sections[section_index] = replace(
            section,
            blocks=selected_blocks,
            hint="; ".join(dict.fromkeys(hints)),
        )

    if any(section.title == "EXAMPLE" for section in sections):
        hide_section("EXAMPLE", f"Example: cxxman {page.topic} --example")
    if manual_page_line_count(current_page()) <= budget:
        return current_page()

    if any(section.title == "SEE ALSO" for section in sections):
        hide_section("SEE ALSO", f"Related names: cxxman {page.topic} --full")
    if manual_page_line_count(current_page()) <= budget:
        return current_page()

    if page.entity_kind == "class":
        if any(section.title == "NON-MEMBERS" for section in sections):
            hide_section("NON-MEMBERS", f"Non-member operations: cxxman {page.topic} --full")
        if manual_page_line_count(current_page()) <= budget:
            return current_page()
        limit_section("MEMBERS", 3, "member group")
    else:
        limit_section("SYNOPSIS", 2, "additional signature")
        if manual_page_line_count(current_page()) <= budget:
            return current_page()
        limit_section("PARAMETERS", 2, "parameter")
        if manual_page_line_count(current_page()) <= budget:
            return current_page()
        limit_section("WATCH OUT", 2, "watch-out")
        if manual_page_line_count(current_page()) <= budget:
            return current_page()
        limit_section("COMPLEXITY", 1, "complexity")

    return current_page()


def _wrapped_line_count(text: str, width: int) -> int:
    return max(1, len(textwrap.wrap(text, width=max(1, width), break_long_words=False)))


def extract_example_document(document: Document) -> Document | None:
    """Return the complete source example section, if it contains code."""
    example_blocks: list[ContentBlock] = []
    in_example = False
    example_level = 0
    for block in document.blocks:
        if isinstance(block, Heading):
            if in_example and block.level <= example_level:
                in_example = False
            if not in_example and block.text.strip().casefold().startswith("example"):
                in_example = True
                example_level = block.level
            continue
        if in_example:
            example_blocks.append(block)
    if not any(isinstance(block, CodeBlock) for block in example_blocks):
        return None
    return Document(
        document.identifier,
        f"Examples for {document.title}",
        document.canonical_topic,
        document.source_url,
        tuple(example_blocks),
        None,
        document.updated_at,
        document.purpose,
        document.aliases,
    )


def _split_sections(
    document: Document,
) -> tuple[list[ContentBlock], dict[str, list[ContentBlock]]]:
    before_sections: list[ContentBlock] = []
    collected: dict[str, list[ContentBlock]] = {}
    current_title: str | None = None
    seen_heading = False
    for block in document.blocks:
        if isinstance(block, Heading):
            if block.level == 1 and _normalize(block.text) == _normalize(document.title):
                continue
            seen_heading = True
            normalized = _normalize(block.text)
            current_title = None if normalized in _EXCLUDED_HEADINGS else _SECTION_TITLES.get(normalized)
            if current_title is not None:
                collected.setdefault(current_title, [])
            continue
        if not seen_heading:
            before_sections.append(block)
        elif current_title is not None:
            collected[current_title].append(block)
    return before_sections, collected


def _select_declarations(
    declarations: list[CodeBlock], entity_kind: str, topic: str
) -> tuple[tuple[ContentBlock, ...], str | None]:
    shown: list[ContentBlock] = []
    hidden_execution: list[CodeBlock] = []
    hidden_ranges: list[CodeBlock] = []
    for declaration in declarations:
        code = declaration.code
        if entity_kind == "algorithm" and "ExecutionPolicy" in code:
            hidden_execution.append(declaration)
        elif entity_kind == "algorithm" and "ranges::" in code:
            hidden_ranges.append(declaration)
        else:
            shown.append(declaration)
    hints: list[str] = []
    if hidden_execution:
        standard = _first_standard(hidden_execution)
        suffix = f" ({standard})" if standard else ""
        hints.append(
            f"+{len(hidden_execution)} ExecutionPolicy overloads{suffix} — cxxman {topic} --full"
        )
    if hidden_ranges:
        hints.append(
            f"+{len(hidden_ranges)} std::ranges overloads — cxxman {topic} --full"
        )
    return tuple(shown), "; ".join(hints) if hints else None


def _parameters(
    blocks: list[ContentBlock], topic: str, declarations: list[CodeBlock]
) -> tuple[tuple[ContentBlock, ...], str | None]:
    tables = _blocks(blocks, Table)
    if not tables:
        return (), None
    has_execution_policy = any("ExecutionPolicy" in block.code for block in declarations)
    rows: list[ContentBlock] = []
    hidden = 0
    hidden_names: list[str] = []
    for table in tables:
        for row in table.rows:
            if not row:
                continue
            name = row[0].strip()
            if name.casefold() in {"deduction guides", "helper classes", "specializations"}:
                continue
            if name.casefold() in {"type requirements", "type requirement"}:
                hidden += 1
                hidden_names.append(name)
                continue
            if has_execution_policy and name.casefold() in {"policy", "executionpolicy"}:
                hidden += 1
                hidden_names.append(name)
                continue
            description = row[-1].strip() if len(row) > 1 else ""
            if not name or not description:
                continue
            passage = Paragraph(f"{name} — {description}")
            if _block_line_count(passage, 96) > 2:
                hidden += 1
                hidden_names.append(name)
                continue
            rows.append(passage)
    details = f" ({', '.join(hidden_names)})" if hidden_names else ""
    hint = f"+{hidden} parameter detail(s){details} — cxxman {topic} --full" if hidden else None
    return tuple(rows), hint


def _template_parameters(
    blocks: list[ContentBlock], topic: str
) -> tuple[tuple[ContentBlock, ...], str | None]:
    selected: list[ContentBlock] = []
    hidden: list[str] = []
    for table in _blocks(blocks, Table):
        for row in table.rows:
            if not row or not row[0].strip():
                continue
            name = row[0].strip()
            description = row[-1].strip() if len(row) > 1 else ""
            passage = Paragraph(f"{name} — {description}" if description else name)
            if _block_line_count(passage, 96) > 2:
                hidden.append(name)
            else:
                selected.append(passage)
    hint = (
        f"+{len(hidden)} template-parameter detail(s) ({', '.join(hidden)}) — cxxman {topic} --full"
        if hidden
        else None
    )
    return tuple(selected), hint


def _short_prose_section(
    blocks: list[ContentBlock], topic: str, label: str
) -> tuple[tuple[ContentBlock, ...], str | None]:
    prose = [block for block in blocks if isinstance(block, (Paragraph, ListBlock))]
    if not prose:
        return (), None
    line_count = sum(_block_line_count(block, 96) for block in prose)
    if line_count <= 3:
        return tuple(prose), None
    hidden = max(0, len(prose) - 1)
    hint = f"More {label}: cxxman {topic} --full"
    if hidden:
        hint = f"+{hidden} {label} block(s) — cxxman {topic} --full"
    return (prose[0],), hint


def _watch_out(
    before_sections: list[ContentBlock],
    source_sections: dict[str, list[ContentBlock]],
    topic: str,
    hidden_overload_labels: set[str],
) -> tuple[tuple[ContentBlock, ...], str | None]:
    selected: list[ContentBlock] = []
    hidden = 0

    invalidation_tables = _blocks(source_sections.get("INVALIDATION", ()), Table)
    remaining_invalidation_rows = 3
    for table in invalidation_tables:
        rows = table.rows[:remaining_invalidation_rows]
        if rows:
            selected.append(Table(rows, headers=table.headers))
            hidden += max(0, len(table.rows) - len(rows))
            remaining_invalidation_rows -= len(rows)

    candidates: list[ContentBlock] = []
    for block in before_sections:
        if isinstance(block, Paragraph) and _contains_important_fact(block.text):
            candidates.extend(_important_sentences(block.text))
    for heading in ("EXCEPTIONS", "REQUIREMENTS"):
        for block in source_sections.get(heading, ()):
            if isinstance(block, Paragraph):
                overloads = _leading_overload_labels(block.text)
                if overloads and overloads <= hidden_overload_labels:
                    hidden += 1
                    continue
                candidates.extend(_important_sentences(block.text, include_all=True))
            elif isinstance(block, Table):
                candidates.extend(_important_table_rows(block))
    undefined_intro = any(
        isinstance(block, Paragraph) and "behavior is undefined" in block.text.casefold()
        for block in before_sections
    )
    if undefined_intro:
        for block in before_sections:
            if isinstance(block, Table):
                candidates.extend(
                    Paragraph(" — ".join(cell.strip() for cell in row if cell.strip()))
                    for row in block.rows
                    if any(cell.strip() for cell in row)
                )

    # The invalidation table is one displayed item; select at most three
    # additional source sentences/rows, preserving their source wording.
    invalidation_item_count = sum(
        len(block.rows) if isinstance(block, Table) else 1 for block in selected
    )
    capacity = max(0, 3 - invalidation_item_count)
    for candidate in candidates[:capacity]:
        selected.append(candidate)
    hidden += max(0, len(candidates) - capacity)

    if not selected:
        return (), None
    hint = f"+{hidden} watch-out item(s) — cxxman {topic} --full" if hidden else None
    return tuple(selected), hint


def _important_sentences(text: str, *, include_all: bool = False) -> list[Paragraph]:
    result = []
    for sentence in _sentences(text):
        if include_all or _contains_important_fact(sentence):
            result.append(Paragraph(sentence))
    return result


def _important_table_rows(table: Table) -> list[Paragraph]:
    result: list[Paragraph] = []
    for row in table.rows:
        text = " — ".join(cell.strip() for cell in row if cell.strip())
        if text and _contains_important_fact(text):
            result.append(Paragraph(text))
    return result


_MEMBER_GROUPS = {
    "element access",
    "iterators",
    "capacity",
    "modifiers",
    "observers",
    "operations",
}


def _looks_like_noisy_revision_row(text: str) -> bool:
    return text.count("(removed in C++") > 1 or text.count("(C++") > 3


def _member_names(
    blocks: list[ContentBlock], topic: str,
) -> tuple[tuple[ContentBlock, ...], str | None]:
    groups: list[tuple[str, list[str]]] = []
    current_group = ""
    current_names: list[str] = []
    omitted_rows = 0
    for table in _blocks(blocks, Table):
        for row in table.rows:
            if not row:
                continue
            name = row[0].strip()
            if name.casefold() in _MEMBER_GROUPS and (len(row) < 2 or not row[1].strip()):
                if current_names:
                    groups.append((current_group, current_names))
                current_group, current_names = name, []
                continue
            if not name or name.casefold() in {
                "member", "name", "group", "deduction guides", "helper classes", "specializations"
            }:
                continue
            if _looks_like_noisy_revision_row(name):
                omitted_rows += 1
                continue
            current_names.append(_strip_member_kind(name))
    if current_names:
        groups.append((current_group, current_names))
    if not groups:
        return (), None
    lines = [
        Paragraph(f"{label}: {', '.join(names)}" if label else ", ".join(names))
        for label, names in groups
    ]
    hint = (
        f"+{omitted_rows} member-name row(s) — cxxman {topic} --full"
        if omitted_rows
        else None
    )
    return tuple(lines), hint


def _see_also(
    blocks: list[ContentBlock], topic: str
) -> tuple[tuple[ContentBlock, ...], str | None]:
    names: list[str] = []
    for block in blocks:
        if isinstance(block, Reference):
            names.append(block.label)
        elif isinstance(block, Table):
            names.extend(row[0].strip() for row in block.rows if row and row[0].strip())
        elif isinstance(block, Paragraph):
            names.append(block.text)
    names = list(dict.fromkeys(names))
    noisy_count = sum(_looks_like_noisy_revision_row(name) for name in names)
    names = [name for name in names if not _looks_like_noisy_revision_row(name)]
    selected: list[str] = []
    for name in names:
        candidate = ", ".join((*selected, name))
        if selected and _block_line_count(Paragraph(candidate), 96) > 2:
            break
        selected.append(name)
    hidden = len(names) - len(selected) + noisy_count
    hint = f"+{hidden} related names — cxxman {topic} --full" if hidden else None
    return ((Paragraph(", ".join(selected)),) if selected else (), hint)


def _example(blocks: list[ContentBlock], topic: str) -> tuple[tuple[ContentBlock, ...], str | None]:
    code_blocks: list[CodeBlock] = []
    output_follows = False
    for block in blocks:
        if isinstance(block, Paragraph) and block.text.strip().casefold() in {
            "output:",
            "possible output:",
        }:
            output_follows = True
            continue
        if isinstance(block, CodeBlock):
            if output_follows:
                continue
            code_blocks.append(block)
    if not code_blocks:
        return (), None
    example = code_blocks[0]
    lines = len(example.code.splitlines())
    if lines > 15:
        return (), f"Example has {lines} lines — cxxman {topic} --example"
    hint = None
    if len(code_blocks) > 1:
        hint = f"+{len(code_blocks) - 1} more example block(s) — cxxman {topic} --example"
    return (example,), hint


def _first_description(paragraphs: list[ContentBlock]) -> Paragraph | None:
    for block in paragraphs:
        if not isinstance(block, Paragraph):
            continue
        text = _without_overload_marker(block.text)
        if not text or _is_numbered_description(block.text):
            continue
        if _is_implementation_note(text) or _contains_important_fact(text):
            continue
        return Paragraph(text)
    return None


def _take_implementation_notes(
    blocks: list[ContentBlock],
) -> tuple[list[Paragraph], list[ContentBlock]]:
    notes: list[Paragraph] = []
    remaining: list[ContentBlock] = []
    for block in blocks:
        if isinstance(block, Paragraph):
            text = _without_overload_marker(block.text)
            if _is_implementation_note(text):
                notes.append(Paragraph(text))
                continue
        elif isinstance(block, ListBlock):
            remaining_items = []
            for item in block.items:
                if _is_implementation_note(item):
                    notes.append(Paragraph(item))
                else:
                    remaining_items.append(item)
            if remaining_items:
                remaining.append(ListBlock(tuple(remaining_items), block.ordered))
            continue
        remaining.append(block)
    return notes, remaining


def _intro_complexity(blocks: list[ContentBlock]) -> list[ContentBlock]:
    for index, block in enumerate(blocks):
        if isinstance(block, Paragraph) and "complexity" in block.text.casefold():
            if index + 1 < len(blocks) and isinstance(blocks[index + 1], ListBlock):
                return [blocks[index + 1]]
    return []


def _first_source_paragraph(blocks: list[ContentBlock]) -> Paragraph | None:
    return next((block for block in blocks if isinstance(block, Paragraph)), None)


def _put(
    sections: dict[str, ManualSection],
    title: str,
    blocks: tuple[ContentBlock, ...] | list[ContentBlock],
    hint: str | None = None,
) -> None:
    nonempty = tuple(block for block in blocks if _block_has_content(block))
    if nonempty or hint:
        sections[title] = ManualSection(title, nonempty, hint)


def _block_has_content(block: ContentBlock) -> bool:
    if isinstance(block, Paragraph):
        return bool(block.text.strip())
    if isinstance(block, CodeBlock):
        return bool(block.code.strip())
    if isinstance(block, ListBlock):
        return any(item.strip() for item in block.items)
    if isinstance(block, Table):
        return bool(block.rows)
    return True


def _blocks(blocks: tuple[ContentBlock, ...] | list[ContentBlock], kind: type) -> list:
    return [block for block in blocks if isinstance(block, kind)]


def _classify_entity(topic: str) -> str:
    normalized = topic.casefold().removeprefix("std::")
    if normalized in _ENTITY_KIND_OVERRIDES:
        return _ENTITY_KIND_OVERRIDES[normalized]
    page = SOURCE_PAGES.get(normalized)
    return page.entity_kind if page is not None else "unknown"


def _normalize(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip().casefold()


def _looks_like_declaration(block: ContentBlock) -> bool:
    if not isinstance(block, CodeBlock):
        return False
    code = block.code.strip()
    if code.startswith("#define "):
        return True
    is_function = "(" in code and ")" in code and ";" in code
    is_type = "template" in code and any(
        keyword in code for keyword in ("class ", "struct ", "using ", "typename ")
    )
    is_object_or_alias = code.endswith(";") and "(" not in code and not code.startswith("#")
    return is_function or is_type or is_object_or_alias


def _without_overload_marker(text: str) -> str:
    return re.sub(r"^\s*\d+(?:\s*,\s*\d+)*\)\s*", "", text, count=1)


def _leading_overload_labels(text: str) -> set[str]:
    match = re.match(r"^\s*(\d+(?:\s*,\s*\d+)*)\)", text)
    if not match:
        return set()
    return {f"({number})" for number in re.findall(r"\d+", match.group(1))}


def _is_numbered_description(text: str) -> bool:
    return _without_overload_marker(text) != text


def _first_sentence(text: str) -> str:
    value = _without_overload_marker(text).strip()
    sentences = _sentences(value)
    return sentences[0] if sentences else value


def _sentences(text: str) -> list[str]:
    value = " ".join(text.split())
    if not value:
        return []
    matches = re.findall(r".*?[.!?](?=\s+[A-Z\"'‘“(\[]|$)|.+$", value)
    return [part.strip() for part in matches if part.strip()]


def _contains_important_fact(text: str) -> bool:
    folded = text.casefold()
    return any(term in folded for term in _IMPORTANT_TERMS)


def _is_implementation_note(text: str) -> bool:
    folded = text.casefold()
    return folded.startswith(tuple(f"in {name.casefold()}" for name in _IMPLEMENTATION_NAMES))


def _strip_member_kind(text: str) -> str:
    return re.sub(
        r"\s*\((?:(?:public|protected|private) member function|class template|function template|algorithm function object|constant|class)\)\s*$",
        "",
        text,
        flags=re.IGNORECASE,
    ).strip()


def _first_standard(blocks: list[CodeBlock]) -> str | None:
    for block in blocks:
        if block.availability:
            match = re.search(r"C\+\+\d+", block.availability)
            if match:
                return match.group(0)
    return None


def _block_line_count(block: ContentBlock, width: int) -> int:
    if isinstance(block, Paragraph):
        return max(1, len(textwrap.wrap(block.text, width=width, break_long_words=False)))
    if isinstance(block, ListBlock):
        return sum(max(1, len(textwrap.wrap(item, width=width - 2))) for item in block.items)
    if isinstance(block, CodeBlock):
        return sum(max(1, len(textwrap.wrap(line, width=width, break_long_words=False))) for line in block.code.splitlines())
    if isinstance(block, Table):
        return sum(max(1, max(len(textwrap.wrap(cell, width=max(12, width // max(1, len(row))))) for cell in row)) for row in block.rows)
    return 1
