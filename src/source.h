#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include "filters.h"

class QJsonObject;
class QNetworkAccessManager;

// One picture, as a gallery describes it. nekos.moe says nothing about
// dimensions, the boorus say everything -- so `size` is filled in when it is
// known and left invalid when it is not.
struct Artwork {
	Provider provider = Provider::NekosMoe;
	QString id;
	QString artist;
	QStringList tags;
	QString imageUrl;
	QString pageUrl;
	QSize size;

	bool isValid() const { return !imageUrl.isEmpty(); }
	QString galleryName() const;
};

// Asks the chosen galleries for pictures, then downloads them one by one and
// keeps the first that fits the wallpaper without losing much of the drawing.
// Asynchronous throughout: the window stays alive while the network takes its
// time, and the headless path waits for the same signal.
class Picker : public QObject {
	Q_OBJECT

public:
	explicit Picker(QSize target, QObject *parent = nullptr);

	// How much of a picture a crop to the target shape throws away, from 0
	// (same shape) to 1. The measure is the shape alone; scaling is free.
	static double cropLoss(QSize art, QSize target);

	// Re-read before every run, so a box ticked in the window takes effect
	// on the next picture rather than on the next launch.
	void setFilters(const Filters &filters) { m_filters = filters; }

	// The size the wallpaper is being drawn for. An invalid size is the
	// "as it comes" setting: then no shape suits better than another and
	// the first picture that downloads wins.
	void setTarget(QSize target) { m_target = target; }

	void start();

signals:
	void progress(const QString &what);
	void picked(const QImage &art, const Artwork &meta);
	void failed(const QString &reason);

private:
	void fetchList(Provider provider);
	void listArrived(Provider provider, const QVector<Artwork> &found, const QString &error);
	void buildQueue();
	void tryNext();
	void finishWithBest();
	bool acceptable(const QStringList &tags, bool adult) const;

	QVector<Artwork> parseNekos(const QByteArray &json) const;
	QVector<Artwork> parseSafebooru(const QByteArray &json) const;
	QVector<Artwork> parseDanbooru(const QByteArray &json) const;

	QNetworkAccessManager *m_network;
	QSize m_target;
	Filters m_filters;

	QHash<int, QVector<Artwork>> m_lists; // by provider, before interleaving
	QHash<int, int> m_attempts;
	QStringList m_errors;
	int m_outstanding = 0;

	QVector<Artwork> m_queue;
	int m_downloaded = 0;

	QImage m_best;
	Artwork m_bestMeta;
	double m_bestLoss = 2.0;

	int m_batch = 20;
	int m_tries = 6;
	double m_goodEnough = 0.25;
};
