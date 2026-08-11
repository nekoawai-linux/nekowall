#include "source.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace {

const char *kApi = "https://nekos.moe/api/v1/random/image";
const char *kImage = "https://nekos.moe/image/";
const char *kPage = "https://nekos.moe/post/";

// A wallpaper is on show to everyone who walks past the machine, so the
// filter starts strict. The nsfw flag of the gallery is the first pass and it
// is a loose one: pictures it calls safe still arrive tagged "large breasts"
// and "garter straps". These tags are matched as substrings, so "breasts"
// covers every size of it. Measured on a batch of 60: the flag removed 27,
// the tags another 11, and 22 came through -- enough for one screen.
// nekowall.conf replaces the whole list for anyone who wants it looser.
const char *kBlockedTags =
	"cleavage,breasts,sideboob,underboob,bikini,swimsuit,lingerie,underwear,"
	"panties,pantsu,bra,garter,nude,naked,nipples,areola,topless,ecchi,"
	"erotic,suggestive,bondage,see-through,wet clothes,undressing,loli,shota";

QNetworkRequest request(const QUrl &url)
{
	QNetworkRequest r(url);
	// Say who is calling: an anonymous flood of requests is how a small
	// gallery ends up blocking a whole distribution.
	r.setHeader(QNetworkRequest::UserAgentHeader,
		QStringLiteral("nekowall/%1 (+https://nekoawai.moe)")
			.arg(QStringLiteral(NEKOWALL_VERSION)));
	// nekos.moe answers in a second or hangs entirely, with no middle
	// ground: waiting longer than this only postpones the retry.
	r.setTransferTimeout(15000);
	return r;
}

const int kListAttempts = 4;

} // namespace

QString Artwork::pageUrl() const { return QString::fromLatin1(kPage) + id; }
QString Artwork::imageUrl() const { return QString::fromLatin1(kImage) + id; }

Picker::Picker(QSize screen, QObject *parent)
	: QObject(parent)
	, m_network(new QNetworkAccessManager(this))
	, m_screen(screen)
{
	QSettings settings(QSettings::IniFormat, QSettings::UserScope,
		QStringLiteral("nekowall"), QStringLiteral("nekowall"));
	m_allowNsfw = settings.value(QStringLiteral("allowNsfw"), false).toBool();
	m_blockedTags = settings
				.value(QStringLiteral("blockedTags"), QString::fromLatin1(kBlockedTags))
				.toString()
				.split(QLatin1Char(','), Qt::SkipEmptyParts);
	m_batch = settings.value(QStringLiteral("batch"), m_batch).toInt();
	m_tries = settings.value(QStringLiteral("tries"), m_tries).toInt();
	for (QString &tag : m_blockedTags)
		tag = tag.trimmed().toLower();
}

double Picker::cropLoss(QSize art, QSize screen)
{
	if (art.isEmpty() || screen.isEmpty())
		return 1.0;
	const double a = double(art.width()) / art.height();
	const double s = double(screen.width()) / screen.height();
	return 1.0 - (a < s ? a / s : s / a);
}

bool Picker::acceptable(const QJsonObject &image) const
{
	if (!m_allowNsfw && image.value(QStringLiteral("nsfw")).toBool(true))
		return false;

	const QJsonArray tags = image.value(QStringLiteral("tags")).toArray();
	for (const QJsonValue &value : tags) {
		const QString tag = value.toString().toLower();
		for (const QString &blocked : m_blockedTags) {
			if (!blocked.isEmpty() && tag.contains(blocked))
				return false;
		}
	}
	return true;
}

void Picker::start()
{
	m_queue.clear();
	m_downloaded = 0;
	m_attempts = 0;
	m_best = QImage();
	m_bestMeta = Artwork();
	m_bestLoss = 2.0;
	fetchCandidates();
}

void Picker::fetchCandidates()
{
	++m_attempts;
	emit progress(m_attempts == 1
			? tr("Asking nekos.moe for pictures...")
			: tr("nekos.moe did not answer. Asking again (%1 of %2)...")
				  .arg(m_attempts)
				  .arg(kListAttempts));

	// The nsfw=false parameter of this endpoint never answers, so the safe
	// pictures are picked out here instead -- which is the more trustworthy
	// place for it anyway.
	QUrl url(QString::fromLatin1(kApi));
	url.setQuery(QStringLiteral("count=%1").arg(m_batch));

	QNetworkReply *reply = m_network->get(request(url));
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			// The gallery answers in a second or hangs, seemingly at
			// random, and one hang at login would leave the desktop
			// bare. Ask again rather than give up on the first one.
			if (m_attempts < kListAttempts) {
				QTimer::singleShot(2000, this, &Picker::fetchCandidates);
				return;
			}
			emit failed(tr("nekos.moe is not answering: %1").arg(reply->errorString()));
			return;
		}

		const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
		const QJsonArray images = root.value(QStringLiteral("images")).toArray();
		for (const QJsonValue &value : images) {
			const QJsonObject image = value.toObject();
			if (!acceptable(image))
				continue;
			Artwork art;
			art.id = image.value(QStringLiteral("id")).toString();
			art.artist = image.value(QStringLiteral("artist")).toString();
			for (const QJsonValue &tag : image.value(QStringLiteral("tags")).toArray())
				art.tags << tag.toString();
			if (art.isValid())
				m_queue << art;
		}

		if (m_queue.isEmpty()) {
			emit failed(tr("Nothing in this batch passed the filter. Try again."));
			return;
		}
		tryNext();
	});
}

void Picker::tryNext()
{
	if (m_queue.isEmpty() || m_downloaded >= m_tries) {
		finishWithBest();
		return;
	}

	const Artwork art = m_queue.takeFirst();
	++m_downloaded;
	emit progress(tr("Looking at picture %1 of at most %2...").arg(m_downloaded).arg(m_tries));

	QNetworkReply *reply = m_network->get(request(QUrl(art.imageUrl())));
	connect(reply, &QNetworkReply::finished, this, [this, reply, art] {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			tryNext();
			return;
		}

		QImage image;
		if (!image.loadFromData(reply->readAll())) {
			tryNext();
			return;
		}

		const double loss = cropLoss(image.size(), m_screen);
		const bool bigEnough = image.width() >= m_screen.width() * 3 / 4;

		// A picture of nearly the screen's shape covers it whole, and that
		// beats anything a backdrop can do -- take the first one and stop.
		if (loss <= m_goodEnough && bigEnough) {
			emit picked(image, art);
			return;
		}

		// Otherwise remember the closest so far: the fallback is the best of
		// what the batch had, not whatever came last.
		if (loss < m_bestLoss || (qFuzzyCompare(loss + 1.0, m_bestLoss + 1.0)
						 && image.width() > m_best.width())) {
			m_best = image;
			m_bestMeta = art;
			m_bestLoss = loss;
		}
		tryNext();
	});
}

void Picker::finishWithBest()
{
	if (m_best.isNull()) {
		emit failed(tr("No picture could be downloaded."));
		return;
	}
	emit picked(m_best, m_bestMeta);
}
