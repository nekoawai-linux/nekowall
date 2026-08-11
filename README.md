<div align="center">

# nekowall

**Random art from [nekos.moe](https://nekos.moe), [safebooru](https://safebooru.org) and [danbooru](https://danbooru.donmai.us) on the desktop, drawn to the size of the screen the machine is actually running.**

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

    nekowall              a window: the picture, and a tab of filters
    nekowall --set        take one and apply it, no window
    nekowall --current    who drew the wallpaper that is set now
    nekowall --version

The **Wallpaper** tab shows what the desktop would look like, who drew it and
where the picture lives, with "another one" and "use as wallpaper".

The **Filters** tab is pills to press, nothing to type:

- **Galleries** -- nekos.moe, safebooru, danbooru, in any combination. Each
  chosen one is asked once and the pictures are taken from them in turn, so
  none of them decides the whole run: three from each of two, two from each of
  three. The last one cannot be switched off.
- **What to search** -- Safe, NSFW, or All. The gallery marks its own
  pictures and that mark is all this chooses by.
- **Wallpaper size** -- Auto, HD, Full HD, 2K, 4K, a size of your own, or
  "any, as it comes", which hands the picture over untouched. Changing it
  redraws the picture that is already on screen; no new download.
- **Show only pictures tagged** -- any ticked tag is enough, and nothing
  ticked means no preference. Ticking tags switches from the random endpoint
  to search.
- **Never show pictures tagged** -- holds in every mode.

Every tag says what each gallery calls it. nekos.moe has its own names,
written in the singular and with spaces; the two boorus share the Danbooru
vocabulary, which is underscored and more specific -- `ocean` rather than
`sea`, `cityscape` rather than `city`. A tag only one side knows is dimmed
while the other is the only gallery chosen.

The nekos.moe column was counted against it before anything got in: it is a
gallery of characters, so `city` had one picture and `sea` none. Scenery
therefore comes from the boorus, which have plenty of it.

A booru reads several tags as *all of them at once*, which with a screenful of
pressed pills would mean no pictures at all. So one of the pressed tags is
drawn at random for each request, which keeps the promise the window makes:
any pressed tag is enough. Danbooru allows an anonymous search two terms, and
the rating is the second one.

The boorus state the width and height of every picture, so the hopeless shapes
are ruled out before a byte is downloaded. nekos.moe says nothing about size,
and those pictures are judged after they arrive.

Ratings differ as well. nekos.moe marks a picture safe or not; danbooru rates
every one of them `g`, `s`, `q` or `e`, and safe means the first; safebooru has
nothing but safe pictures, so in the NSFW mode it is not asked at all.

`nekowall.service` is a systemd user unit that runs `--set` once, at the first
login of a system that has no wallpaper yet. After that the choice is yours;
the unit stays out of the way.

Xfce, GNOME, KDE Plasma, Hyprland and Niri are set through their own tools
(`xfconf-query`, `gsettings`, `plasma-apply-wallpaperimage`, `hyprctl` with
`hyprpaper`, `swaybg`). Any other Wayland session gets `swaybg` as well.

## What it will not show

Each gallery marks its own pictures, and that mark is the first pass. It is a
loose one -- nekos.moe pictures called safe still arrive tagged `large breasts`
or `garter straps` -- so a tag list is checked as well, against every picture
from every gallery. Out of a nekos.moe batch of 60, the flag removed 27 and the
tags another 11. Booru tags are read with underscores turned back into spaces,
so a booru's spelling cannot slip past the list.

Some tags are refused before any of that, in every mode, with no pill to press
and no key in the settings file. Child content is not a preference to
configure.

A wallpaper is on show to everyone who walks past the machine, which is why
the default is strict rather than clever. It is not a promise: the filter can
only work with the tags an uploader typed.

## Settings

`~/.config/nekowall/nekowall.ini` -- written by the Filters tab, and read by
`--set`. Editing it by hand works too:

    [General]
    galleries=nekos.moe, safebooru, danbooru   ; any combination
    mode=safe           ; safe, nsfw, all
    size=auto           ; auto, 1280x720, 1920x1080, 2560x1440, 3840x2160,
                        ; custom, any
    customWidth=1920
    customHeight=1080
    wantedTags=cat ears, kimono
    blockedTags=...     ; comma separated, matched inside tag names
    batch=20            ; pictures asked for at once
    tries=6             ; pictures downloaded before settling for the best so far

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

## Manners

Every request says `nekowall/<version>` and who to complain to. The gallery is
small and free; one wallpaper at login and whatever the user asks for by hand
is the whole of it.

The pictures belong to the artists who drew them. nekowall stores one file at
a time, names the artist next to it, and links back to the page.

## License

Copyright (c) 2026 shizukiq. GPL-3.0-or-later; see `LICENSE`.
