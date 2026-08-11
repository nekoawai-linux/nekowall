#pragma once

#include <QHash>
#include <QImage>
#include <QSize>
#include <QWidget>

#include "filters.h"
#include "source.h"

class QLabel;
class QPushButton;
class QSpinBox;

// Everything here is a pill to press: galleries, mode, size, tags. Nothing is
// typed, because neither gallery has an endpoint that would tell us whether a
// typed tag exists.
class FiltersTab : public QWidget {
	Q_OBJECT

public:
	explicit FiltersTab(QWidget *parent = nullptr);
	Filters filters() const { return m_filters; }

signals:
	void changed(const Filters &filters);

private:
	QWidget *row(const QVector<QPair<int, QString>> &choices, int current,
		const std::function<void(int)> &chosen, int columns = 4);
	QWidget *galleryPills();
	QWidget *wantedGrid();
	QWidget *blockedGrid();
	void updateTagAvailability();
	void collect();

	Filters m_filters;
	QHash<QString, QPushButton *> m_galleries;
	QHash<QString, QPushButton *> m_wanted;
	QHash<QString, QPushButton *> m_blocked;
	QSpinBox *m_customWidth = nullptr;
	QSpinBox *m_customHeight = nullptr;
	QLabel *m_summary = nullptr;
};

// One picture at a time: what the desktop would look like, who drew it, and
// two buttons.
class Window : public QWidget {
	Q_OBJECT

public:
	explicit Window(QWidget *parent = nullptr);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	void shuffle();
	void showCandidate(const QImage &art, const Artwork &meta);
	void render();
	void applyCurrent();
	void updatePreview();
	void setBusy(bool busy);

	QSize m_screen;
	QSize m_target;
	Picker *m_picker;
	FiltersTab *m_filters;

	QLabel *m_preview;
	QLabel *m_credit;
	QLabel *m_status;
	QPushButton *m_another;
	QPushButton *m_apply;

	QImage m_art;
	QImage m_canvas;
	Artwork m_meta;
	bool m_cropped = false;
};
