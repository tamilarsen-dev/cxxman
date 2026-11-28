#include "app/completions.hpp"

#include "app/options.hpp"

#include <algorithm>

namespace cxxman::app {

namespace {

// Options that take one of a few words are completed by the script itself;
// options that take free text (--width, -H, --kind, ...) complete nothing.
// Everything else asks `cxxman --complete`.

constexpr std::string_view bash = R"sh(# cxxman completion for bash. Load it with:
#   eval "$(cxxman --completions bash)"
_cxxman() {
  local cur=${COMP_WORDS[COMP_CWORD]} prev=${COMP_WORDS[COMP_CWORD-1]} words
  case $prev in
    --color|--pager) words='auto always never' ;;
    --theme) words='dark light mono' ;;
    --std|--new|--since) words='11 14 17 20 23 26' ;;
    --completions) words='bash zsh fish powershell' ;;
    --width|--header|-H|--missing|--kind|--copy|--complete) return ;;
  esac
  if [[ -n $words ]]; then
    COMPREPLY=($(compgen -W "$words" -- "$cur"))
    return
  fi
  # bash splits words at ':'; complete the whole name, then drop the part
  # before the last ':' that bash already has.
  local word=${COMP_LINE:0:COMP_POINT}
  word=${word##*[[:space:]]}
  local IFS=$'\n'
  COMPREPLY=($(cxxman --complete "$word" 2>/dev/null))
  if [[ $word == *:* ]]; then
    local head=${word%"${word##*:}"}
    COMPREPLY=("${COMPREPLY[@]#"$head"}")
  fi
}
complete -F _cxxman cxxman
)sh";

constexpr std::string_view zsh = R"sh(#compdef cxxman
# cxxman completion for zsh. Load it with (after compinit):
#   eval "$(cxxman --completions zsh)"
_cxxman() {
  case ${words[CURRENT-1]} in
    --color|--pager) compadd -- auto always never; return ;;
    --theme) compadd -- dark light mono; return ;;
    --std|--new|--since) compadd -- 11 14 17 20 23 26; return ;;
    --completions) compadd -- bash zsh fish powershell; return ;;
    --width|--header|-H|--missing|--kind|--copy|--complete) return ;;
  esac
  local -a names
  names=(${(f)"$(cxxman --complete "$PREFIX" 2>/dev/null)"})
  compadd -- $names
}
compdef _cxxman cxxman
)sh";

constexpr std::string_view fish = R"sh(# cxxman completion for fish. Load it with:
#   cxxman --completions fish | source
complete -c cxxman -f -a '(cxxman --complete (commandline -ct) 2>/dev/null)'
complete -c cxxman -l color -x -a 'auto always never'
complete -c cxxman -l pager -x -a 'auto always never'
complete -c cxxman -l theme -x -a 'dark light mono'
complete -c cxxman -l std -x -a '11 14 17 20 23 26'
complete -c cxxman -l new -x -a '11 14 17 20 23 26'
complete -c cxxman -l since -x -a '11 14 17 20 23 26'
complete -c cxxman -l completions -x -a 'bash zsh fish powershell'
)sh";

constexpr std::string_view powershell = R"sh(# cxxman completion for PowerShell. Load it with:
#   cxxman --completions powershell | Out-String | Invoke-Expression
Register-ArgumentCompleter -Native -CommandName cxxman, cxxman.exe -ScriptBlock {
  param($wordToComplete, $commandAst, $cursorPosition)
  $before = $commandAst.CommandElements |
    Where-Object { $_.Extent.EndOffset -lt $cursorPosition -or
                   ($_.Extent.EndOffset -eq $cursorPosition -and -not $wordToComplete) }
  $prev = if ($before) { "$(@($before)[-1])" } else { '' }
  $words = switch ($prev) {
    { $_ -in '--color', '--pager' } { 'auto', 'always', 'never' }
    '--theme' { 'dark', 'light', 'mono' }
    { $_ -in '--std', '--new', '--since' } { '11', '14', '17', '20', '23', '26' }
    '--completions' { 'bash', 'zsh', 'fish', 'powershell' }
    { $_ -in '--width', '--header', '-H', '--missing', '--kind', '--copy' } { return }
  }
  if ($words) {
    $words = $words | Where-Object { $_.StartsWith($wordToComplete) }
  } else {
    $words = cxxman --complete "$wordToComplete" 2>$null
  }
  foreach ($w in $words) {
    # Quote names that PowerShell would read as syntax: operator<<, operator[].
    $text = if ($w -match '[\s<>|&;(){}\[\]@$#,''"`]') { "'" + ($w -replace "'", "''") + "'" } else { $w }
    [System.Management.Automation.CompletionResult]::new($text, $w, 'ParameterValue', $w)
  }
}
)sh";

std::string fold(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

}  // namespace

std::string_view completion_script(std::string_view shell) {
  if (shell == "bash") return bash;
  if (shell == "zsh") return zsh;
  if (shell == "fish") return fish;
  if (shell == "powershell") return powershell;
  return {};
}

std::string complete(const Catalog& cat, std::string_view prefix) {
  std::string out;
  if (prefix.starts_with('-')) {
    for (auto name : option_names())
      if (name.starts_with(prefix)) (out += name) += '\n';
    return out;
  }
  // Keys are stored without std::; give it back so the shell keeps the word.
  std::string_view lead;
  if (fold(prefix.substr(0, 5)) == "std::") {
    lead = prefix.substr(0, 5);
    prefix.remove_prefix(5);
  }
  const std::string folded = fold(prefix);
  std::string_view last;
  for (auto it = std::ranges::lower_bound(cat.keys, std::string_view(folded), {}, &Key::folded);
       it != cat.keys.end() && it->folded.starts_with(folded); ++it) {
    if (it->target != Target::Entry && it->target != Target::Index) continue;
    if (it->key == last) continue;  // one line per name: the table is sorted by key too
    last = it->key;
    ((out += lead) += it->key) += '\n';
  }
  return out;
}

}  // namespace cxxman::app
