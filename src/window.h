#pragma once

#include <QImage>
#include <QSize>
#include <QWidget>

#include "filters.h"
#include "source.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QSpinBox;

// Boxes to tick, no tags to type: the gallery has no endpoint that lists its
// tags, so the offered ones are written down and measured, and everything
// here is a choice between them.
class FiltersTab : public QWidget {
	Q_OBJECT

public:
	explicit FiltersTab(QWidget *parent = nullptr);
	Filters filters() const { return m_filters; }

signals:
	void changed(const Filters &filters);

private:
	QWidget *tagBoxes(const QStringList &tags, const QStringList &ticked,
		QList<QCheckBox *> *into);
	QWidget *sizeChoices();
	void collect();

	Filters m_filters;
	QList<QCheckBox *> m_wanted;
	QList<QCheckBox *> m_blocked;
	QSpinBox *m_customWidth;
	QSpinBox *m_customHeight;
	QLabel *m_summary;
};

// One picture at a time: what it would look like on the desktop, who drew it,
// and two buttons.
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
