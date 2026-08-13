<div align="center">

# nekowall

=^..^=

![License](https://img.shields.io/badge/License-GPL--3.0--or--later-green?style=for-the-badge)
![Qt](https://img.shields.io/badge/Qt-6-41cd52?style=for-the-badge&logo=qt&logoColor=white)
![Desktops](https://img.shields.io/badge/Desktops-Xfce_·_GNOME_·_Plasma_·_Hyprland_·_Niri-9b7fd4?style=for-the-badge)
[![Website](https://img.shields.io/badge/Website-nekoawai.moe-%23e32b6b?style=for-the-badge)](https://nekoawai.moe)

**A wallpaper that changes with you.**

Random art from three galleries, drawn to the exact size of the screen the
machine is running. Written for [NekoAwai](https://github.com/nekoawai-linux/nekoawai-linux),
at home on any desktop that will take a picture.

</div>

## What it does

Asks the galleries you chose for pictures, keeps the ones that pass your
filters, downloads them until one suits the screen, and builds a wallpaper of
exactly the screen's resolution:

- a picture of nearly the screen's shape is scaled to cover it;
- anything else -- and a lot of art is portrait -- is laid whole on a blurred,
  darkened copy of itself, so nothing is cut off and desktop icons stay
  readable.

The resolution comes from `/sys/class/drm`, so the answer is the same under
X11, under Wayland, and with no session at all. With several screens the
canvas is made for the largest.

The artist is recorded next to the wallpaper. `nekowall --current` prints who
drew what is on screen and links back to it.

## Three galleries

| | Tags | Sizes | Ratings |
| --- | --- | --- | --- |
| [nekos.moe](https://nekos.moe) | thousands, in its own words | not published | safe or not |
| [safebooru](https://safebooru.org) | the Danbooru vocabulary | published | safe only |
| [danbooru](https://danbooru.donmai.us) | the Danbooru vocabulary | published | `g` `s` `q` `e` |

Any combination of them, and the last one cannot be switched off. Each chosen
gallery is asked once and the pictures are taken from them in turn, so none of
them decides the whole run: three from each of two, two from each of three.

Because the boorus publish width and height, the hopeless shapes are ruled out
before a byte is downloaded. nekos.moe says nothing about size, so those are
judged after they arrive.

## The window

    nekowall              the picture, the filters, and the autostart
    nekowall --set        take one and apply it, no window
    nekowall --set --at-login   the same, if the autostart setting says so
    nekowall --current    who drew the wallpaper that is set now
    nekowall --version

**Wallpaper** shows what the desktop would look like, who drew it, and two
buttons. **Filters** is pills to press, nothing to type:

- **Galleries** -- which of the three, in any combination.
- **What to search** -- Safe, NSFW or All. Each gallery marks its own pictures
  and that mark is all this chooses by. Safebooru has no adult pictures at
  all, so in the NSFW mode it is not asked.
- **Wallpaper size** -- Auto, HD, Full HD, 2K, 4K, a size of your own, or
  *any, as it comes*, which hands the picture over untouched. Changing it
  redraws the picture already on screen; no new download.
- **Show pictures tagged** -- any pressed tag is enough. A tag only one side
  knows is dimmed while the other is the only gallery chosen.
- **Never show pictures tagged** -- holds in every mode.

Every tag says what each gallery calls it. nekos.moe writes tags in the
singular and with spaces; the boorus share the Danbooru vocabulary, which is
underscored and more specific -- `ocean` rather than `sea`, `cityscape` rather
than `city`. The nekos.moe column was counted against the gallery before
anything got in: it is a gallery of characters, so `city` had one picture and
`sea` none. Scenery comes from the boorus instead.

A booru reads several tags as *all of them at once*, which with a screenful of
pressed pills would mean no pictures at all. So one pressed tag is drawn at
random for each request, which keeps the promise the window makes. Danbooru
allows an anonymous search two terms, and the rating is the second.

## Autostart

The third tab is what nekowall does when nobody asks it to. Two questions,
and they are not the same one:

- **At login** -- never, only the first time, or every login. Only the first
  time is the default and what a fresh install wants: a desktop that comes up
  grey says nothing about the system it belongs to, and once there is a
  wallpaper the choice belongs to whoever is sitting there.
- **While the session runs** -- never, hourly, every 3 or 6 hours, or daily.

`nekowall.service` is the login unit. It is enabled once, for everyone, when
the package is installed, and it runs `--set --at-login`, which reads the
setting above and may well leave the wallpaper alone. So the answer lives in
one file a person can read rather than in a symlink somewhere under
`~/.config/systemd`.

The interval is a timer, because nothing else wakes a program up in six
hours. `nekowall-rotate.timer` is enabled the moment a pill is pressed, with
the interval written to
`~/.config/systemd/user/nekowall-rotate.timer.d/interval.conf`. It is bound to
`graphical-session.target`: it runs while the desktop does and stops with it,
and a machine somebody is logged into over ssh has no desktop to change.

Where there are no units -- a build directory rather than a package -- or no
systemd user manager, the timer pills are dimmed and the tab says which of
the two is missing.

Xfce, GNOME, KDE Plasma, Hyprland and Niri are each set through their own tool
(`xfconf-query`, `gsettings`, `plasma-apply-wallpaperimage`, `hyprctl` with
`hyprpaper`, `swaybg`). Any other Wayland session gets `swaybg` as well -- and
is checked for still running a moment later, because a `swaybg` that exits at
once is a wallpaper that was never set.

## What it will not show

Each gallery marks its own pictures, and that mark is the first pass. It is a
loose one -- nekos.moe pictures called safe still arrive tagged `large breasts`
-- so a tag list is checked as well, against every picture from every gallery.
Out of a nekos.moe batch of 60, the flag removed 27 and the tags another 11.
Booru tags are read with underscores turned back into spaces, so a booru's
spelling cannot slip past the list.

Child content is refused before all of that, in every mode, with no pill to
press and no key in the settings file. It is not a preference to configure.

A wallpaper is on show to everyone who walks past the machine, which is why
the default is strict rather than clever. It is not a promise either: the
filter can only work with the tags an uploader typed.

## Settings

`~/.config/nekowall/nekowall.ini` -- written by the Filters and Autostart
tabs, read by `--set`. Editing it by hand works too:

    [General]
    galleries=nekos.moe, safebooru, danbooru   ; any combination
    mode=safe           ; safe, nsfw, all
    size=auto           ; auto, 1280x720, 1920x1080, 2560x1440, 3840x2160,
                        ; custom, any
    customWidth=1920
    customHeight=1080
    wantedTags=cat ears, scenery
    blockedTags=...     ; comma separated, matched inside tag names
    batch=20            ; pictures asked for at once
    tries=6             ; pictures downloaded before settling for the best so far
    autostart=once      ; off, once, login -- what --at-login does
    changeEvery=0       ; minutes between pictures; 0, 60, 180, 360 or 1440.
                        ; The window enables the timer as well; this key
                        ; alone only says what it was asked for.

## Building

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Qt 6 (Core, Gui, Widgets, Network) is the only dependency: network, JSON and
image work all come from it, so there is no curl, no jq and no ImageMagick to
install. The window draws its own dark theme rather than borrowing the
desktop's widget style -- nekowall runs on five desktops, and a picker that
looks like a control panel on one and a form on another belongs to none.

    make check     configure, build, --version
    make dist      reproducible release archive

The archive is what [nekoawai-linux](https://github.com/nekoawai-linux/nekoawai-linux)
packages as the `nekowall` RPM. `patterns-nekoawai-desktop-base` recommends
it, so every NekoAwai desktop has it unless the installer was told otherwise,
and the minimal profile does not.

## Manners

Every request says `nekowall/<version>` and where to complain. The galleries
are small and free; one wallpaper at login and whatever is asked for by hand
is the whole of it. A gallery that answers with a refusal is not asked again
in the same run.

The pictures belong to the artists who drew them. nekowall keeps one file at a
time, names the artist next to it, and links back to the original.

## License

Copyright (c) 2026 shizukiq. GPL-3.0-or-later; see `LICENSE`.

<sub><sub>[donate in TON](https://tonviewer.com/UQAj-bErFKSDkHqy_5RSwkKxmkE3RgATMLFHp-TYX5JN2kHe)</sub></sub>
