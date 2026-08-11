<div align="center">

# nekowall

**Random art from [nekos.moe](https://nekos.moe) on the desktop, drawn to the size of the screen the machine is actually running.**

</div>

## What it does

Asks nekos.moe for a batch of random pictures, keeps the ones that pass the
filter, downloads them until one suits the screen, and builds a wallpaper of
exactly the screen's resolution:

- a picture of nearly the screen's shape is scaled to cover it;
- anything else -- and most art there is portrait -- is laid whole on a
  blurred, darkened copy of itself, so nothing is cut off and desktop icons
  stay readable.

The resolution comes from `/sys/class/drm`, so the answer is the same under
X11, under Wayland, and with no session at all. With several screens the
canvas is made for the largest.

The artist is recorded next to the wallpaper. `nekowall --current` prints who
drew what is on screen and links to its page.

## Running it

    nekowall              a window: preview, "another one", "use as wallpaper"
    nekowall --set        take one and apply it, no window
    nekowall --current    who drew the wallpaper that is set now
    nekowall --version

`nekowall.service` is a systemd user unit that runs `--set` once, at the first
login of a system that has no wallpaper yet. After that the choice is yours;
the unit stays out of the way.

Xfce, GNOME, KDE Plasma, Hyprland and Niri are set through their own tools
(`xfconf-query`, `gsettings`, `plasma-apply-wallpaperimage`, `hyprctl` with
`hyprpaper`, `swaybg`). Any other Wayland session gets `swaybg` as well.

## What it will not show

nekos.moe marks adult pictures itself, and that flag is the first pass. It is
a loose one -- pictures it calls safe still arrive tagged `large breasts` or
`garter straps` -- so a tag list is checked as well. Out of a batch of 60, the
flag removed 27 and the tags another 11.

A wallpaper is on show to everyone who walks past the machine, which is why
the default is strict rather than clever. It is not a promise: the filter can
only work with the tags an uploader typed.

## Settings

`~/.config/nekowall/nekowall.conf`, all optional:

    [General]
    allowNsfw=false     ; the gallery's own flag
    blockedTags=...     ; comma separated, matched inside tag names
    batch=20            ; pictures asked for at once
    tries=6             ; pictures downloaded before settling for the best so far

## Building

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Qt 6 (Core, Gui, Widgets, Network) is the only dependency: network, JSON and
image work all come from it, so there is no curl, no jq and no ImageMagick to
install.

    make check     configure, build, --version
    make dist      reproducible release archive

## Manners

Every request says `nekowall/<version>` and who to complain to. The gallery is
small and free; one wallpaper at login and whatever the user asks for by hand
is the whole of it.

The pictures belong to the artists who drew them. nekowall stores one file at
a time, names the artist next to it, and links back to the page.

## License

Copyright (c) 2026 shizukiq. GPL-3.0-or-later; see `LICENSE`.
