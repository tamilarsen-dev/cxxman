#!/bin/sh
set -eu

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_executable="$script_directory/cxxman"
source_uninstaller="$script_directory/uninstall.sh"
install_directory="$HOME/.local/bin"
if [ ! -f "$source_executable" ] || [ ! -f "$source_uninstaller" ]; then
    echo "cxxman and uninstall.sh must be next to install.sh. Extract the full release archive first." >&2
    exit 1
fi

mkdir -p "$install_directory"
install -m 755 "$source_executable" "$install_directory/cxxman"
install -m 755 "$source_uninstaller" "$install_directory/uninstall.sh"

case "${SHELL##*/}" in
    zsh) profile="$HOME/.zshrc" ;;
    bash) profile="$HOME/.bashrc" ;;
    fish) profile="$HOME/.config/fish/config.fish" ;;
    *) profile="$HOME/.profile" ;;
esac
path_line='export PATH="$HOME/.local/bin:$PATH"'
if [ "${SHELL##*/}" = fish ]; then
    path_line='fish_add_path "$HOME/.local/bin"'
fi
mkdir -p "$(dirname -- "$profile")"
if ! grep -Fqx "$path_line" "$profile" 2>/dev/null; then
    printf '\n%s\n' "$path_line" >> "$profile"
fi

printf 'Installed cxxman to %s\n' "$install_directory"
printf 'Open a new terminal, then run: cxxman --help\n'
printf 'To remove it later, run: sh "%s/uninstall.sh"\n' "$install_directory"
