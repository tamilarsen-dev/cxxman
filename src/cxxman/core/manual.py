"""Conservative projection from parsed reference documents to manual pages."""

from __future__ import annotations

from dataclasses import dataclass
import re

from cxxman.core.catalog import SOURCE_PAGES
from cxxman.core.document import CodeBlock, ContentBlock, Document, Heading, Reference


@dataclass(frozen=True, slots=True)
class ManualSection:
    """A non-empty, display-ready manual section."""

    title: str
    blocks: tuple[ContentBlock, ...]


@dataclass(frozen=True, slots=True)
class ManualPage:
    """A concise view of a generic parsed document."""

    name: str
    entity_kind: str
    header: str | None
    source_url: str
    sections: tuple[ManualSection, ...]


_SECTION_TITLES = {
    "description": "DESCRIPTION",
    "overview": "DESCRIPTION",
    "member functions": "MEMBER FUNCTIONS",
    "observers": "OBSERVERS",
    "member types": "MEMBER TYPES",
    "template parameters": "TEMPLATE PARAMETERS",
    "parameters": "PARAMETERS",
    "return value": "RETURN VALUE",
    "returns": "RETURN VALUE",
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
    "iterators": "ITERATORS",
    "capacity": "CAPACITY",
    "element access": "ELEMENT ACCESS",
    "modifiers": "MODIFIERS",
    "notes": "NOTES",
    "example": "EXAMPLES",
    "examples": "EXAMPLES",
    "see also": "SEE ALSO",
    "references": "REFERENCES",
    "synopsis": "SYNOPSIS",
    "declarations": "SYNOPSIS",
}

_SECTION_ORDER = (
    "DESCRIPTION",
    "SYNOPSIS",
    "TEMPLATE PARAMETERS",
    "MEMBER TYPES",
    "MEMBER FUNCTIONS",
    "OBSERVERS",
    "ITERATORS",
    "CAPACITY",
    "ELEMENT ACCESS",
    "MODIFIERS",
    "PARAMETERS",
    "RETURN VALUE",
    "COMPLEXITY",
    "REQUIREMENTS",
    "EXCEPTIONS",
    "STANDARDS",
    "INVALIDATION",
    "NOTES",
    "EXAMPLES",
    "SEE ALSO",
    "REFERENCES",
)
_ORDER_INDEX = {title: index for index, title in enumerate(_SECTION_ORDER)}


def project_document(document: Document) -> ManualPage:
    """Project recognized source sections without inventing technical facts.

    Generic blocks remain stored in the original ``Document``. Unrecognized
    article headings are omitted from this concise view rather than guessed.
    """
    entity_kind = _classify_entity(document.canonical_topic)
    collected: dict[str, list[ContentBlock]] = {}
    current_title: str | None = None
    before_sections: list[ContentBlock] = []
    seen_section = False

    for block in document.blocks:
        if isinstance(block, Heading):
            if (
                block.level == 1
                and _normalize(block.text) == _normalize(document.title)
            ):
                continue
            current_title = _SECTION_TITLES.get(_normalize(block.text))
            if current_title is not None:
                seen_section = True
                collected.setdefault(current_title, [])
            continue

        if not seen_section:
            before_sections.append(block)
        elif current_title is not None:
            collected[current_title].append(block)

    description: list[ContentBlock] = []
    synopsis: list[ContentBlock] = []
    see_also: list[ContentBlock] = []
    for block in before_sections:
        if isinstance(block, Reference):
            see_also.append(block)
        elif _looks_like_declaration(block):
            synopsis.append(block)
        else:
            description.append(block)
    if description:
        collected.setdefault("DESCRIPTION", [])[:0] = description
    if synopsis:
        collected.setdefault("SYNOPSIS", [])[:0] = synopsis
    if see_also:
        collected.setdefault("SEE ALSO", []).extend(see_also)

    sections = [
        ManualSection(title, tuple(blocks))
        for title, blocks in collected.items()
        if blocks
    ]
    sections.sort(
        key=lambda section: (
            _ORDER_INDEX.get(section.title, len(_ORDER_INDEX)),
            section.title,
        )
    )

    return ManualPage(
        name=document.title,
        entity_kind=entity_kind,
        header=document.header,
        source_url=document.source_url,
        sections=tuple(sections),
    )


def _classify_entity(topic: str) -> str:
    normalized = topic.casefold().removeprefix("std::")
    page = SOURCE_PAGES.get(normalized)
    return page.entity_kind if page is not None else "unknown"


def _normalize(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip().casefold()


def _looks_like_declaration(block: ContentBlock) -> bool:
    if not isinstance(block, CodeBlock):
        return False
    code = block.code.strip()
    is_function = "(" in code and ")" in code and ";" in code
    is_type = "template" in code and any(
        keyword in code for keyword in ("class ", "struct ", "using ", "typename ")
    )
    return is_function or is_type
