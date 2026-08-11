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
#include <QUrlQuery>

#include <algorithm>

namespace {

const char *kNekosRandom = "https://nekos.moe/api/v1/random/image";
const char *kNekosSearch = "https://nekos.moe/api/v1/images/search";
const char *kNekosImage = "https://nekos.moe/image/";
const char *kNekosPage = "https://nekos.moe/post/";
const char *kWaifuSearch = "https://api.waifu.im/search";

const int kListAttempts = 4;

QNetworkRequest request(const QUrl &url)
{
	QNetworkRequest r(url);
	// Say who is calling: an anonymous flood of requests is how a small
	// gallery ends up blocking a whole distribution.
	r.setHeader(QNetworkRequest::UserAgentHeader,
		QStringLiteral("nekowall/%1 (+https://nekoawai.moe)")
			.arg(QStringLiteral(NEKOWALL_VERSION)));
	// The galleries answer in a second or hang entirely, with no middle
	// ground: waiting longer than this only postpones the retry.
	r.setTransferTimeout(15000);
	return r;
}

} // namespace

QString Artwork::galleryName() const
{
	return provider == Provider::NekosMoe ? QStringLiteral("nekos.moe")
					      : QStringLiteral("waifu.im");
}

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

double Picker::cropLoss(QSize art, QSize target)
{
	if (art.isEmpty() || target.isEmpty())
		return 1.0;
	const double a = double(art.width()) / art.height();
	const double s = double(target.width()) / target.height();
	return 1.0 - (a < s ? a / s : s / a);
}

bool Picker::acceptable(const QStringList &tags, bool adult) const
{
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
	for (const QString &raw : tags) {
		const QString tag = raw.toLower();
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
	m_lists.clear();
	m_attempts.clear();
	m_errors.clear();
	m_downloaded = 0;
	m_best = QImage();
	m_bestMeta = Artwork();
	m_bestLoss = 2.0;

	const QVector<Provider> providers = m_filters.providers();
	m_outstanding = providers.size();
	// Both galleries are asked at once, and the queue afterwards takes from
	// them in turn: with the usual budget of six downloads that is three
	// pictures from each.
	for (Provider provider : providers)
		fetchList(provider);
}

void Picker::fetchList(Provider provider)
{
	const int key = int(provider);
	const int attempt = m_attempts.value(key) + 1;
	m_attempts[key] = attempt;

	const QString gallery = provider == Provider::NekosMoe
		? QStringLiteral("nekos.moe")
		: QStringLiteral("waifu.im");
	emit progress(attempt == 1
			? tr("Asking %1 for pictures...").arg(gallery)
			: tr("%1 did not answer. Asking again (%2 of %3)...")
				  .arg(gallery)
				  .arg(attempt)
				  .arg(kListAttempts));

	const QStringList tags = m_filters.tagsFor(provider);
	QNetworkReply *reply = nullptr;

	if (provider == Provider::NekosMoe) {
		if (tags.isEmpty()) {
			// The nsfw parameter of the random endpoint never answers,
			// which is why the mode is applied to the records instead.
			QUrl url(QString::fromLatin1(kNekosRandom));
			url.setQuery(QStringLiteral("count=%1").arg(m_batch));
			reply = m_network->get(request(url));
		} else {
			QJsonObject body;
			body[QStringLiteral("tags")] = QJsonArray::fromStringList(tags);
			body[QStringLiteral("limit")] = m_batch;
			// Search always answers from the front of the list, so
			// without a random start the same pictures come for ever.
			body[QStringLiteral("skip")] = QRandomGenerator::global()->bounded(60);
			if (m_filters.mode == Filters::Safe)
				body[QStringLiteral("nsfw")] = false;
			else if (m_filters.mode == Filters::Adult)
				body[QStringLiteral("nsfw")] = true;

			QNetworkRequest post = request(QUrl(QString::fromLatin1(kNekosSearch)));
			post.setHeader(QNetworkRequest::ContentTypeHeader,
				QStringLiteral("application/json"));
			reply = m_network->post(post,
				QJsonDocument(body).toJson(QJsonDocument::Compact));
		}
	} else {
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("limit"), QString::number(qMin(m_batch, 30)));
		for (const QString &tag : tags)
			query.addQueryItem(QStringLiteral("included_tags"), tag);
		switch (m_filters.mode) {
		case Filters::Safe:
			query.addQueryItem(QStringLiteral("is_nsfw"), QStringLiteral("false"));
			break;
		case Filters::Adult:
			query.addQueryItem(QStringLiteral("is_nsfw"), QStringLiteral("true"));
			break;
		case Filters::Everything:
			query.addQueryItem(QStringLiteral("is_nsfw"), QStringLiteral("null"));
			break;
		}
		// waifu.im knows the shape of its pictures, so the hopeless ones
		// need never be downloaded to be ruled out.
		if (m_target.isValid()) {
			query.addQueryItem(QStringLiteral("orientation"),
				m_target.width() >= m_target.height()
					? QStringLiteral("LANDSCAPE")
					: QStringLiteral("PORTRAIT"));
		}

		QUrl url(QString::fromLatin1(kWaifuSearch));
		url.setQuery(query);
		reply = m_network->get(request(url));
	}

	connect(reply, &QNetworkReply::finished, this, [this, reply, provider, gallery] {
		reply->deleteLater();
		const int status
			= reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

		if (reply->error() != QNetworkReply::NoError) {
			// The galleries answer in a second or hang, seemingly at
			// random, and one hang at login would leave the desktop
			// bare. Ask again rather than give up on the first one --
			// but a refusal is an answer, and repeating it is rude.
			if (m_attempts.value(int(provider)) < kListAttempts && status != 403) {
				QTimer::singleShot(2000, this,
					[this, provider] { fetchList(provider); });
				return;
			}
			listArrived(provider, {},
				status == 403
					? tr("%1 refused the request (403): the gateway in "
					     "front of it does not allow this network.")
						  .arg(gallery)
					: tr("%1 is not answering: %2")
						  .arg(gallery, reply->errorString()));
			return;
		}

		const QByteArray body = reply->readAll();
		const QVector<Artwork> found = provider == Provider::NekosMoe
			? parseNekos(body)
			: parseWaifu(body);
		if (found.isEmpty() && !body.trimmed().startsWith('{')) {
			listArrived(provider, {},
				tr("%1 answered with something that is not a picture list.")
					.arg(gallery));
			return;
		}
		listArrived(provider, found, QString());
	});
}

QVector<Artwork> Picker::parseNekos(const QByteArray &json) const
{
	QVector<Artwork> found;
	const QJsonArray images
		= QJsonDocument::fromJson(json).object().value(QStringLiteral("images")).toArray();
	for (const QJsonValue &value : images) {
		const QJsonObject image = value.toObject();
		QStringList tags;
		for (const QJsonValue &tag : image.value(QStringLiteral("tags")).toArray())
			tags << tag.toString();
		if (!acceptable(tags, image.value(QStringLiteral("nsfw")).toBool(true)))
			continue;

		Artwork art;
		art.provider = Provider::NekosMoe;
		art.id = image.value(QStringLiteral("id")).toString();
		if (art.id.isEmpty())
			continue;
		art.artist = image.value(QStringLiteral("artist")).toString();
		art.tags = tags;
		art.imageUrl = QString::fromLatin1(kNekosImage) + art.id;
		art.pageUrl = QString::fromLatin1(kNekosPage) + art.id;
		found << art;
	}
	return found;
}

QVector<Artwork> Picker::parseWaifu(const QByteArray &json) const
{
	QVector<Artwork> found;
	const QJsonArray images
		= QJsonDocument::fromJson(json).object().value(QStringLiteral("images")).toArray();
	for (const QJsonValue &value : images) {
		const QJsonObject image = value.toObject();
		QStringList tags;
		for (const QJsonValue &tag : image.value(QStringLiteral("tags")).toArray())
			tags << tag.toObject().value(QStringLiteral("name")).toString();
		if (!acceptable(tags, image.value(QStringLiteral("is_nsfw")).toBool(true)))
			continue;

		Artwork art;
		art.provider = Provider::WaifuIm;
		art.id = QString::number(image.value(QStringLiteral("image_id")).toInt());
		art.artist = image.value(QStringLiteral("artist"))
				     .toObject()
				     .value(QStringLiteral("name"))
				     .toString();
		art.tags = tags;
		art.imageUrl = image.value(QStringLiteral("url")).toString();
		// The source is where the drawing actually lives -- pixiv, twitter
		// -- which is a better credit than the gallery's copy of it.
		art.pageUrl = image.value(QStringLiteral("source")).toString();
		if (art.pageUrl.isEmpty())
			art.pageUrl = art.imageUrl;
		art.size = QSize(image.value(QStringLiteral("width")).toInt(),
			image.value(QStringLiteral("height")).toInt());
		if (art.isValid())
			found << art;
	}
	return found;
}

void Picker::listArrived(Provider provider, const QVector<Artwork> &found, const QString &error)
{
	if (!error.isEmpty())
		m_errors << error;
	m_lists[int(provider)] = found;
	if (--m_outstanding <= 0)
		buildQueue();
}

void Picker::buildQueue()
{
	// One from each in turn, so neither gallery decides the whole run.
	const QVector<Provider> providers = m_filters.providers();
	int longest = 0;
	for (Provider provider : providers)
		longest = qMax(longest, m_lists.value(int(provider)).size());

	for (int i = 0; i < longest; ++i) {
		for (Provider provider : providers) {
			const QVector<Artwork> &list = m_lists[int(provider)];
			if (i < list.size())
				m_queue << list.at(i);
		}
	}

	if (m_queue.isEmpty()) {
		emit failed(m_errors.isEmpty()
				? tr("Nothing came back that passed the filters.")
				: m_errors.join(QStringLiteral(" ")));
		return;
	}

	// waifu.im states the size of every picture it has, so the ones that
	// would be downloaded only to be rejected go last without a byte spent
	// on them. Pictures of unknown size keep their place in the middle.
	if (m_target.isValid()) {
		std::stable_sort(m_queue.begin(), m_queue.end(),
			[this](const Artwork &a, const Artwork &b) {
				const double la
					= a.size.isValid() ? cropLoss(a.size, m_target) : 0.30;
				const double lb
					= b.size.isValid() ? cropLoss(b.size, m_target) : 0.30;
				return la < lb;
			});
	}
	tryNext();
}

void Picker::tryNext()
{
	if (m_queue.isEmpty() || m_downloaded >= m_tries) {
		finishWithBest();
		return;
	}

	const Artwork art = m_queue.takeFirst();
	++m_downloaded;
	emit progress(tr("Looking at picture %1 of at most %2, from %3...")
			      .arg(m_downloaded)
			      .arg(m_tries)
			      .arg(art.galleryName()));

	QNetworkReply *reply = m_network->get(request(QUrl(art.imageUrl)));
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

		// A picture of nearly the target shape covers it whole, and that
		// beats anything a backdrop can do -- take the first one and stop.
		if (loss <= m_goodEnough && bigEnough) {
			emit picked(image, art);
			return;
		}

		// Otherwise remember the closest so far: the fallback is the best of
		// what came back, not whatever came last.
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
		emit failed(m_errors.isEmpty() ? tr("No picture could be downloaded.")
					       : m_errors.join(QStringLiteral(" ")));
		return;
	}
	emit picked(m_best, m_bestMeta);
}
