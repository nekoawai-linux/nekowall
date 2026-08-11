#pragma once

#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

// Where pictures come from. The two galleries have almost nothing in common:
// nekos.moe has thousands of tags and no dimensions, waifu.im has twenty tags
// and knows the size of every picture. Everything else in the program is
// written so that neither of those facts leaks out of this file and source.cpp.
enum class Provider { NekosMoe, WaifuIm };

// One box to tick, and what each gallery calls it. An empty name means that
// gallery does not know the tag -- there is no pretending otherwise, so the
// box is simply out of reach while that gallery is the only one chosen.
struct TagChoice {
	QString label;
	QString nekos;
	QString waifu;

	bool knownTo(Provider provider) const
	{
		return provider == Provider::NekosMoe ? !nekos.isEmpty() : !waifu.isEmpty();
	}
};

// What the user is asking for, in one place: the picker, the window and the
// headless run must agree, and the file on disk is what they agree through.
struct Filters {
	enum Mode {
		Safe, // only what the gallery marks as safe
		Adult, // only what it marks as adult
		Everything // whatever comes
	};

	// The size of the finished wallpaper. Auto is the screen the machine is
	// running; AsItComes hands the picture over untouched, for a desktop
	// that would rather scale it itself.
	enum Size { Auto, HD, FullHD, WQHD, UHD, Custom, AsItComes };

	enum Where { Nekos, Waifu, BothGalleries };

	Mode mode = Safe;
	Size size = Auto;
	Where where = Nekos;
	QSize custom = QSize(1920, 1080);
	QStringList wanted; // labels from the table below; empty means anything
	QStringList blocked; // none of these tags, whatever the mode

	// The canvas to draw, given what the kernel reports. An invalid size
	// means "leave the picture as it is".
	QSize target(QSize screen) const;
	QVector<Provider> providers() const;
	// The tags to ask this gallery for, in its own vocabulary.
	QStringList tagsFor(Provider provider) const;

	static Filters load();
	void save() const;

	static QString modeKey(Mode mode);
	static Mode modeFromKey(const QString &key);
	static QString sizeKey(Size size);
	static Size sizeFromKey(const QString &key);
	static QString whereKey(Where where);
	static Where whereFromKey(const QString &key);

	// The tags offered as boxes to tick, with what each gallery calls them.
	// Written down rather than fetched: nekos.moe has no endpoint that lists
	// tags, and every entry here was counted against both galleries before
	// it got in. A box that always comes back empty is worse than no box.
	static QVector<TagChoice> tagTable();

	// Ticked by default. The galleries' own safe flags let these through.
	static QStringList defaultBlockedTags();

	// Refused in every mode, from every gallery, with no box to untick and
	// no key in the settings file. Not a preference to configure.
	static QStringList alwaysBlockedTags();
};
