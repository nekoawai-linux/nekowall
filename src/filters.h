#pragma once

#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

// Where pictures come from. Three galleries with three habits: nekos.moe has
// its own tag names and says nothing about dimensions; the two boorus share
// the Danbooru vocabulary, state the size of every picture and rate every one
// of them. Everything else in the program is written so that none of that
// leaks out of this file and source.cpp.
enum class Provider { NekosMoe, Safebooru, Danbooru };

// One box to tick, and what each gallery calls it. The two boorus share a
// vocabulary, so one name covers both. An empty name means that gallery does
// not know the tag -- there is no pretending otherwise, so the box is out of
// reach while only that gallery is chosen.
struct TagChoice {
	QString label;
	QString nekos;
	QString booru;

	bool knownTo(Provider provider) const
	{
		return provider == Provider::NekosMoe ? !nekos.isEmpty() : !booru.isEmpty();
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

	Mode mode = Safe;
	Size size = Auto;
	// Any combination, never empty: the pills are a set, not a choice of one.
	QStringList galleries = { QStringLiteral("nekos.moe") };
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
	static QString galleryKey(Provider provider);
	static Provider galleryFromKey(const QString &key);
	static QVector<Provider> allGalleries();

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
