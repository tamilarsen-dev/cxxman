# Writing cxxman cards

This is the contract for every card, whoever writes it. The five files in
`data/entries/` are the reference; when in doubt, match them. Schema and
hard limits: `docs/PLAN.md` §5. The linter enforces the limits; this
file covers what the linter can't check.

## 1. The reader

A working C++ developer in the middle of a task. They know what a
template and an iterator are. They forgot the argument order, the header,
whether it invalidates iterators, or which of three similar functions
they want. Every line must help with one of those questions within
seconds. The full story is on cppreference, one `--web` away.

## 2. Summary

- One line, ≤ 72 characters, ending with a period.
- Say what it does and the one thing that distinguishes it from its
  neighbors: "Sort a range in ascending order. Not stable." and "Remove
  consecutive duplicates. Does not shrink the container."
- For a function, start with a verb in imperative form ("Sort",
  "Remove", "Find"). For a type, use a noun phrase ("Dynamic array with
  contiguous storage.").
- No "This function…", no "Used to…", no marketing ("powerful",
  "efficient").

## 3. Signatures

Call shapes, not declarations. Notation rules:

- Write the return type as `-> type` only when it matters to the caller
  (`unique(...) -> iterator`). Omit it for `void` and obvious cases.
- Show defaults: `ranges::sort(r, comp = {}, proj = {})`.
- Classes: `vector<T>`, `map<Key, T>`. Drop allocator, comparator and
  hash parameters unless users commonly set them.
- Collapse overload families: ExecutionPolicy overloads become the badge
  `parallel C++17`; `const`/non-`const` pairs become one line.

Also:

- Parameter names are the conventional ones: `first, last`, `r`
  (range), `pos`, `pred`, `comp`, `proj`, `value`, `n`, `key`, `f`.
- Show what the caller passes and gets back, not how it is constrained.
  Concepts and `requires` clauses go only into a note, and only when they
  explain a common compile error ("needs random-access iterators").
- 1–5 lines. Overload families collapse into one line plus a note or a
  badge.

## 4. Examples

- Lead with the most common use. The second example covers the next most
  common variation; the third shows the modern (ranges / C++20+) form or
  the fix for the top gotcha.
- Use idiomatic modern C++: `auto` where obvious, braced init,
  range-for, and lambdas. Always qualify with `std::`; never write
  `using namespace std`.
- Use short conventional names: `v` (vector), `s` (string), `m` (map),
  `it`, `n`, `x`. Supporting setup goes in `test.prelude`/`test.global`,
  which is not shown on the card, so the visible code stays one to three
  lines.
- The comment says the *intent* ("descending", "drop all dups"), not a
  restatement of the code.
- Every example must compile under the CI flags. No example may rely on
  undefined behavior, even if it would compile.
- Write code that teaches the right habit. Show the erase–remove idiom,
  not a manual loop with an index bug.

## 5. Gotchas

Gotchas are the most valuable lines on a card, and the easiest to get
wrong.

- Only include what actually bites people: crashes, UB, silent wrong
  results, surprising complexity, iterator invalidation, and "this
  isn't what you think" (`vector<bool>`, `optional<bool>`, `map::operator[]`
  inserting).
- `ub` means undefined behavior or a guaranteed crash. `warn` means
  well-defined but surprising. `perf` means a cost people don't expect.
- Each gotcha states the problem *and* the fix or alternative where one
  exists: "...: use stable_sort to keep it".
- Never write generic advice ("be careful with iterators", "check for
  errors"). If you cannot name the specific trap, leave it out.
- No gotcha is better than an invented one.

## 6. Accuracy and sources

- Check every fact against cppreference or the standard draft
  (https://eel.is/c++draft/): header, `since`, badges, complexity,
  return types, exceptions, and invalidation rules.
- **Write in your own words.** Never paste or lightly edit cppreference
  sentences. Their text is CC BY-SA and incompatible with this project's
  data license. Facts aren't copyrightable; phrasing is.
- `since` is the standard that introduced the entity. Later changes go
  in `badges` (`constexpr C++20`), signature `since`, or member-group
  `since`.
- If you are not sure about a fact, leave it out and write
  `unverified: <what>` in the commit message so the owner can check it.
  A smaller card that is correct beats a fuller one that might not be.
- Agents: no prior knowledge counts as verified for numbers
  (complexity, limits), version badges, or exception guarantees. If you
  cannot check them, omit them.

### Review checklist (owner, per card)

1. Is the header correct? Is `since` correct?
2. Does the summary distinguish it from its nearest neighbor?
3. Do the signatures cover what 90% of callers use, with nothing
   misleading?
4. Are the examples idiomatic, with the most common use first?
5. Is every gotcha real and specific, with a fix where one exists?
6. Is there no sentence copied from cppreference?
7. Do the tags match what a dev would actually type to find it?

## 7. Tags and aliases

- Tags are search words, in English: synonyms, the task ("remove
  duplicates", "split string", "read file"), and names from other
  languages people search for (`dedupe`, `nullable`, `hashmap`,
  `dictionary`).
- Aliases are exact alternative lookup keys: `ranges::` variants,
  `_if`/`_n` siblings that share the card, and member spellings
  (`vector::push_back` ↔ nothing else). An alias must not collide with
  another card's id or alias (the linter checks).
- Sharing cards: closely related names with one shared shape belong on
  one card with aliases (`find`, `find_if`, `find_if_not`; `all_of`,
  `any_of`, `none_of`; `stoi`, `stol`, `stoll`). Write separate cards
  when the behavior differs in ways that need their own gotchas.

## 8. Class cards and member cards

- A class card lists members in at most six groups, names only. Write
  a member card only when the member has its own gotchas, overloads or
  complexity worth knowing (`vector::erase`: yes; `vector::empty`: no).
- A member card's examples assume the class card's obvious setup in
  `test.prelude`.

## 9. Recipes

- A recipe answers "how do I <task>?" when the answer spans several
  entities or the obvious entity isn't obvious (split, trim, read a file).
  If one card's examples already answer the task, add tags to that card
  instead of writing a recipe.
- Variants are real alternatives, ordered by how often you'd pick them,
  each titled by *when* to pick it: "Owning parts, any standard",
  "Non-owning views, no allocation". At most three.
- Every variant compiles (PLAN §5.5). Keep each one ≤ 8 lines. Setup goes
  in `test.prelude`.
- Gotchas follow §5 and are about the task, not any one function.

## 10. Compares

- A compare answers "which one do I use?". Rows hold only the
  dimensions that drive that choice (order, complexity, requirements,
  invalidation, ownership, cost). Use 3–8 rows.
- Cells are terse facts, not sentences: `O(log n)`, `sorted by key`,
  `nothing`. The facts are verified like card facts (§6).
- Verdicts are actionable rules of thumb that start with a verb:
  "Default to…", "Use X when…". At most three. They are the one place
  where judgment is allowed; keep it mainstream and defensible, not a
  personal taste.

## 11. Support data and verification fields

- `[support]` values (minimum GCC/Clang/MSVC/AppleClang versions,
  feature-test macro and value) come only from cppreference's compiler-
  support and feature-test pages via `tools/upstream/sync_support.py`, or
  from checking them by hand on those pages. Never guess them. A partial
  `[support]` table is fine; a wrong one isn't.
- `verified` is the date you actually checked every fact on the item
  against upstream. Set it only via `tools/upstream/accept.py`, which
  also records the upstream revision. Do not hand-edit the date.
