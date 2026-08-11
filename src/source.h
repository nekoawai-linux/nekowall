#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include "filters.h"

class QJsonObject;
class QNetworkAccessManager;

// One picture, the way nekos.moe describes it. The catalogue carries no
// dimensions, so how well a picture suits the screen is only known once the
// file itself is here.
struct Artwork {
	QString id;
	QString artist;
	QStringList tags;

	bool isValid() const { return !id.isEmpty(); }
	QString pageUrl() const;
	QString imageUrl() const;
};

// Asks for a batch of pictures, then downloads them one by one and keeps the
// first that fits the screen without losing much of the drawing.
// Asynchronous throughout: the window stays alive while the network takes its
// time, and the headless path waits for the same signal.
class Picker : public QObject {
	Q_OBJECT

public:
	explicit Picker(QSize target, QObject *parent = nullptr);

	// How much of a picture a crop to the screen shape throws away, from 0
	// (same shape) to 1. The measure is the shape alone; scaling is free.
	static double cropLoss(QSize art, QSize screen);

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
	void fetchCandidates();
	void handleCandidates(const QByteArray &json);
	void tryNext();
	void finishWithBest();
	bool acceptable(const QJsonObject &image) const;

	QNetworkAccessManager *m_network;
	QSize m_target;
	Filters m_filters;
	QVector<Artwork> m_queue;
	int m_downloaded = 0;
	int m_attempts = 0;
	int m_skip = 0;

	QImage m_best;
	Artwork m_bestMeta;
	double m_bestLoss = 2.0;

	int m_batch = 20;
	int m_tries = 6;
	double m_goodEnough = 0.25;
};
