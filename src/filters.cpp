#include "filters.h"

#include <QSettings>

namespace {

QSettings settings()
{
	return QSettings(QSettings::IniFormat, QSettings::UserScope,
		QStringLiteral("nekowall"), QStringLiteral("nekowall"));
}

} // namespace

QString Filters::modeKey(Mode mode)
{
	switch (mode) {
	case Adult:
		return QStringLiteral("nsfw");
	case Everything:
		return QStringLiteral("all");
	case Safe:
		break;
	}
	return QStringLiteral("safe");
}

Filters::Mode Filters::modeFromKey(const QString &key)
{
	if (key == QLatin1String("nsfw"))
		return Adult;
	if (key == QLatin1String("all"))
		return Everything;
	return Safe;
}

QString Filters::sizeKey(Size size)
{
	switch (size) {
	case HD:
		return QStringLiteral("1280x720");
	case FullHD:
		return QStringLiteral("1920x1080");
	case WQHD:
		return QStringLiteral("2560x1440");
	case UHD:
		return QStringLiteral("3840x2160");
	case Custom:
		return QStringLiteral("custom");
	case AsItComes:
		return QStringLiteral("any");
	case Auto:
		break;
	}
	return QStringLiteral("auto");
}

Filters::Size Filters::sizeFromKey(const QString &key)
{
	if (key == QLatin1String("1280x720"))
		return HD;
	if (key == QLatin1String("1920x1080"))
		return FullHD;
	if (key == QLatin1String("2560x1440"))
		return WQHD;
	if (key == QLatin1String("3840x2160"))
		return UHD;
	if (key == QLatin1String("custom"))
		return Custom;
	if (key == QLatin1String("any"))
		return AsItComes;
	return Auto;
}

QString Filters::whereKey(Where where)
{
	switch (where) {
	case Waifu:
		return QStringLiteral("waifu.im");
	case BothGalleries:
		return QStringLiteral("both");
	case Nekos:
		break;
	}
	return QStringLiteral("nekos.moe");
}

Filters::Where Filters::whereFromKey(const QString &key)
{
	if (key == QLatin1String("waifu.im"))
		return Waifu;
	if (key == QLatin1String("both"))
		return BothGalleries;
	return Nekos;
}

QSize Filters::target(QSize screen) const
{
	switch (size) {
	case HD:
		return QSize(1280, 720);
	case FullHD:
		return QSize(1920, 1080);
	case WQHD:
		return QSize(2560, 1440);
	case UHD:
		return QSize(3840, 2160);
	case Custom:
		return custom;
	case AsItComes:
		// Nothing to draw: the picture goes to the desktop as it was
		// downloaded, and scaling it is the desktop's business.
		return QSize();
	case Auto:
		break;
	}
	return screen;
}

QVector<Provider> Filters::providers() const
{
	switch (where) {
	case Waifu:
		return { Provider::WaifuIm };
	case BothGalleries:
		return { Provider::NekosMoe, Provider::WaifuIm };
	case Nekos:
		break;
	}
	return { Provider::NekosMoe };
}

QStringList Filters::tagsFor(Provider provider) const
{
	QStringList out;
	for (const TagChoice &choice : tagTable()) {
		if (!wanted.contains(choice.label))
			continue;
		const QString name = provider == Provider::NekosMoe ? choice.nekos : choice.waifu;
		if (!name.isEmpty())
			out << name;
	}
	return out;
}

Filters Filters::load()
{
	QSettings store = settings();
	Filters filters;
	filters.mode = modeFromKey(store.value(QStringLiteral("mode"),
					     modeKey(Safe)).toString());
	filters.size = sizeFromKey(store.value(QStringLiteral("size"),
					     sizeKey(Auto)).toString());
	filters.where = whereFromKey(store.value(QStringLiteral("gallery"),
					      whereKey(Nekos)).toString());
	filters.custom = QSize(store.value(QStringLiteral("customWidth"), 1920).toInt(),
		store.value(QStringLiteral("customHeight"), 1080).toInt());
	filters.wanted = store.value(QStringLiteral("wantedTags")).toStringList();
	// An absent list is the default one; an empty list is a user who ticked
	// every box off, and that is a different thing.
	filters.blocked = store.contains(QStringLiteral("blockedTags"))
		? store.value(QStringLiteral("blockedTags")).toStringList()
		: defaultBlockedTags();
	return filters;
}

void Filters::save() const
{
	QSettings store = settings();
	store.setValue(QStringLiteral("mode"), modeKey(mode));
	store.setValue(QStringLiteral("size"), sizeKey(size));
	store.setValue(QStringLiteral("gallery"), whereKey(where));
	store.setValue(QStringLiteral("customWidth"), custom.width());
	store.setValue(QStringLiteral("customHeight"), custom.height());
	// An empty list would be written as @Invalid(), which is Qt talking to
	// itself in a file a person is meant to be able to read.
	if (wanted.isEmpty())
		store.remove(QStringLiteral("wantedTags"));
	else
		store.setValue(QStringLiteral("wantedTags"), wanted);
	store.setValue(QStringLiteral("blockedTags"), blocked);
}

QVector<TagChoice> Filters::tagTable()
{
	// Counted against both galleries on 11 August 2026. nekos.moe writes
	// tags in the singular -- "flower" has a gallery's worth, "flowers" has
	// none -- and it is a gallery of characters rather than places, so no
	// scenery is offered: "city" had one picture and "sea" none.
	//
	// waifu.im has twenty tags in total, and its explicit ones are left out
	// on purpose: the mode above already says whether adult pictures are
	// wanted, and there is no reason to spell them out on a settings page.
	return {
		{ QStringLiteral("cat ears"), QStringLiteral("cat ears"), {} },
		{ QStringLiteral("fox ears"), QStringLiteral("fox ears"), {} },
		{ QStringLiteral("animal ears"), QStringLiteral("animal ears"), {} },
		{ QStringLiteral("tail"), QStringLiteral("tail"), {} },
		{ QStringLiteral("cat"), QStringLiteral("cat"), {} },
		{ QStringLiteral("long hair"), QStringLiteral("long hair"), {} },
		{ QStringLiteral("short hair"), QStringLiteral("short hair"), {} },
		{ QStringLiteral("twintails"), QStringLiteral("twintails"), {} },
		{ QStringLiteral("blue eyes"), QStringLiteral("blue eyes"), {} },
		{ QStringLiteral("smile"), QStringLiteral("smile"), {} },
		{ QStringLiteral("sitting"), QStringLiteral("sitting"), {} },
		{ QStringLiteral("kimono"), QStringLiteral("kimono"), {} },
		{ QStringLiteral("dress"), QStringLiteral("dress"), {} },
		{ QStringLiteral("sweater"), QStringLiteral("sweater"), {} },
		{ QStringLiteral("hoodie"), QStringLiteral("hoodie"), {} },
		{ QStringLiteral("ribbon"), QStringLiteral("ribbon"), {} },
		{ QStringLiteral("hat"), QStringLiteral("hat"), {} },
		{ QStringLiteral("glasses"), QStringLiteral("glasses"), {} },
		{ QStringLiteral("flower"), QStringLiteral("flower"), {} },
		{ QStringLiteral("star"), QStringLiteral("star"), {} },
		{ QStringLiteral("sky"), QStringLiteral("sky"), {} },
		{ QStringLiteral("food"), QStringLiteral("food"), {} },
		{ QStringLiteral("one piece"), QStringLiteral("one piece"), {} },
		// Known to both, in their own words.
		{ QStringLiteral("uniform"), QStringLiteral("school uniform"),
			QStringLiteral("uniform") },
		{ QStringLiteral("maid"), QStringLiteral("maid"), QStringLiteral("maid") },
		{ QStringLiteral("rem"), QStringLiteral("rem"), QStringLiteral("rem") },
		// waifu.im only.
		{ QStringLiteral("any waifu"), {}, QStringLiteral("waifu") },
		{ QStringLiteral("selfies"), {}, QStringLiteral("selfies") },
		{ QStringLiteral("genshin impact"), {}, QStringLiteral("genshin-impact") },
		{ QStringLiteral("raiden shogun"), {}, QStringLiteral("raiden-shogun") },
		{ QStringLiteral("marin kitagawa"), {}, QStringLiteral("marin-kitagawa") },
		{ QStringLiteral("mori calliope"), {}, QStringLiteral("mori-calliope") },
		{ QStringLiteral("kamisato ayaka"), {}, QStringLiteral("kamisato-ayaka") },
	};
}

QStringList Filters::defaultBlockedTags()
{
	return {
		QStringLiteral("cleavage"),
		QStringLiteral("breasts"),
		QStringLiteral("oppai"),
		QStringLiteral("sideboob"),
		QStringLiteral("underboob"),
		QStringLiteral("ass"),
		QStringLiteral("bikini"),
		QStringLiteral("swimsuit"),
		QStringLiteral("lingerie"),
		QStringLiteral("underwear"),
		QStringLiteral("panties"),
		QStringLiteral("pantsu"),
		QStringLiteral("bra"),
		QStringLiteral("garter"),
		QStringLiteral("nude"),
		QStringLiteral("naked"),
		QStringLiteral("nipples"),
		QStringLiteral("areola"),
		QStringLiteral("topless"),
		QStringLiteral("ecchi"),
		QStringLiteral("ero"),
		QStringLiteral("hentai"),
		QStringLiteral("erotic"),
		QStringLiteral("suggestive"),
		QStringLiteral("bondage"),
		QStringLiteral("see-through"),
		QStringLiteral("wet clothes"),
		QStringLiteral("undressing"),
	};
}

QStringList Filters::alwaysBlockedTags()
{
	// These never reach a screen, whatever the mode says and whatever the
	// settings file says. They are here rather than among the tick boxes
	// because a tick box is an offer, and this is not on offer.
	//
	// waifu.im has no tag of this kind in its twenty -- its vocabulary is
	// listed in tagTable above -- so nothing of its own needs adding. The
	// check runs against the tag names of every picture from either
	// gallery, which is what makes it a floor rather than a list.
	return {
		QStringLiteral("loli"),
		QStringLiteral("shota"),
		QStringLiteral("toddlercon"),
		QStringLiteral("child"),
		QStringLiteral("underage"),
	};
}
