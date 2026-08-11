#pragma once

#include <QImage>
#include <QSize>
#include <QWidget>

#include "source.h"

class QLabel;
class QPushButton;

// One picture at a time: what it would look like on the desktop, who drew it,
// and two buttons. Everything else belongs in nekowall.conf.
class Window : public QWidget {
	Q_OBJECT

public:
	explicit Window(QWidget *parent = nullptr);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	void shuffle();
	void showCandidate(const QImage &art, const Artwork &meta);
	void applyCurrent();
	void updatePreview();
	void setBusy(bool busy);

	QSize m_screen;
	Picker *m_picker;

	QLabel *m_preview;
	QLabel *m_credit;
	QLabel *m_status;
	QPushButton *m_another;
	QPushButton *m_apply;

	QImage m_canvas;
	Artwork m_meta;
	bool m_cropped = false;
};
