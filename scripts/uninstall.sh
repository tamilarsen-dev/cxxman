#!/bin/sh
set -eu

install_directory="$HOME/.local/bin"
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

if [ -f "$profile" ]; then
    temporary_profile=$(mktemp)
    grep -Fvx "$path_line" "$profile" > "$temporary_profile" || true
    cat "$temporary_profile" > "$profile"
    rm -f "$temporary_profile"
fi
rm -f "$install_directory/cxxman" "$install_directory/uninstall.sh"

printf 'Removed cxxman and its PATH entry. The local documentation database was kept.\n'
printf 'Open a new terminal for the PATH change to take effect.\n'
