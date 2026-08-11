#include "source.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace {

const char *kRandom = "https://nekos.moe/api/v1/random/image";
const char *kSearch = "https://nekos.moe/api/v1/images/search";
const char *kImage = "https://nekos.moe/image/";
const char *kPage = "https://nekos.moe/post/";

const int kListAttempts = 4;

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

} // namespace

QString Artwork::pageUrl() const { return QString::fromLatin1(kPage) + id; }
QString Artwork::imageUrl() const { return QString::fromLatin1(kImage) + id; }

Picker::Picker(QSize target, QObject *parent)
	: QObject(parent)
	, m_network(new QNetworkAccessManager(this))
	, m_target(target)
	, m_filters(Filters::load())
{
	QSettings settings(QSettings::IniFormat, QSettings::UserScope,
		QStringLiteral("nekowall"), QStringLiteral("nekowall"));
	m_batch = settings.value(QStringLiteral("batch"), m_batch).toInt();
	m_tries = settings.value(QStringLiteral("tries"), m_tries).toInt();
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
	const bool adult = image.value(QStringLiteral("nsfw")).toBool(true);
	switch (m_filters.mode) {
	case Filters::Safe:
		if (adult)
			return false;
		break;
	case Filters::Adult:
		if (!adult)
			return false;
		break;
	case Filters::Everything:
		break;
	}

	// Two lists. The one the user ticked holds in every mode, and the one
	// nobody can untick holds before it.
	const QStringList refused = Filters::alwaysBlockedTags() + m_filters.blocked;
	const QJsonArray tags = image.value(QStringLiteral("tags")).toArray();
	for (const QJsonValue &value : tags) {
		const QString tag = value.toString().toLower();
		for (const QString &blocked : refused) {
			const QString needle = blocked.trimmed().toLower();
			if (!needle.isEmpty() && tag.contains(needle))
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
	// Search answers from the front of the list every time, so without a
	// random start the same pictures come back for ever.
	m_skip = QRandomGenerator::global()->bounded(80);
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

	QNetworkReply *reply = nullptr;
	if (m_filters.wanted.isEmpty()) {
		// Nothing in particular is wanted, so take whatever comes. The
		// nsfw parameter of this endpoint never answers, which is why the
		// mode is applied to the records instead.
		QUrl url(QString::fromLatin1(kRandom));
		url.setQuery(QStringLiteral("count=%1").arg(m_batch));
		reply = m_network->get(request(url));
	} else {
		// Tags were ticked, and search takes them -- any of them, not all,
		// so a screenful of boxes widens the choice instead of narrowing
		// it to nothing. Here the mode does travel with the request.
		QJsonObject body;
		body[QStringLiteral("tags")] = QJsonArray::fromStringList(m_filters.wanted);
		body[QStringLiteral("limit")] = m_batch;
		body[QStringLiteral("skip")] = m_skip;
		if (m_filters.mode == Filters::Safe)
			body[QStringLiteral("nsfw")] = false;
		else if (m_filters.mode == Filters::Adult)
			body[QStringLiteral("nsfw")] = true;

		QNetworkRequest post = request(QUrl(QString::fromLatin1(kSearch)));
		post.setHeader(QNetworkRequest::ContentTypeHeader,
			QStringLiteral("application/json"));
		reply = m_network->post(post, QJsonDocument(body).toJson(QJsonDocument::Compact));
	}

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
		handleCandidates(reply->readAll());
	});
}

void Picker::handleCandidates(const QByteArray &json)
{
	const QJsonObject root = QJsonDocument::fromJson(json).object();
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

	if (!m_queue.isEmpty()) {
		tryNext();
		return;
	}

	// A random start past the end of a small tag is an empty answer rather
	// than an error: begin again from the front before giving up.
	if (m_skip > 0 && m_attempts < kListAttempts) {
		m_skip = 0;
		fetchCandidates();
		return;
	}
	if (m_attempts < kListAttempts && m_filters.wanted.isEmpty()) {
		fetchCandidates();
		return;
	}
	emit failed(m_filters.wanted.isEmpty()
			? tr("Nothing in this batch passed the filters.")
			: tr("Nothing matches these tags in this mode. Tick fewer tags, or "
			     "change the mode."));
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

		// Without a canvas to fill there is nothing to be picky about.
		if (!m_target.isValid()) {
			emit picked(image, art);
			return;
		}

		const double loss = cropLoss(image.size(), m_target);
		const bool bigEnough = image.width() >= m_target.width() * 3 / 4;

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
