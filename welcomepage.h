#pragma once
#include "catalog.h"

#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>
#include <functional>

class WelcomePage : public QWidget
{
public:
    std::function<void(int)> onChoose;       // reçoit ModElectronics, ModElectrotech ou ModElectromag

    explicit WelcomePage(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        // images facultatives (voir resources/) : electronique, electrotechnique, electromagnetique
        for (int i = 0; i < 3; ++i)
            for (const char *ext : {"jpg", "png"}) {
                const QString path = QString(":/img/%1.%2").arg(kInfo[i].img, ext);
                if (QFile::exists(path)) {
                    m_img[i] = QPixmap(path);
                    break;
                }
            }
    }

protected:
    void showEvent(QShowEvent *) override { setFocus(); }

    void leaveEvent(QEvent *) override
    {
        m_hover = -1;
        unsetCursor();
        update();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        const int i = cardAt(e->position());
        if (i != m_hover) {
            m_hover = i;
            setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        const int i = cardAt(e->position());
        if (i >= 0)
            choose(i);
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        switch (e->key()) {
        case Qt::Key_Left:  m_hover = m_hover <= 0 ? 2 : m_hover - 1; update(); break;
        case Qt::Key_Right: m_hover = m_hover < 0 || m_hover >= 2 ? 0 : m_hover + 1; update(); break;
        case Qt::Key_1: choose(0); break;
        case Qt::Key_2: choose(1); break;
        case Qt::Key_3: choose(2); break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_hover >= 0)
                choose(m_hover);
            break;
        default: QWidget::keyPressEvent(e);
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const qreal W = width(), H = height();

        QLinearGradient bg(0, 0, 0, H);
        bg.setColorAt(0, QColor(150, 154, 160));      // gris clair en haut
        bg.setColorAt(1, QColor(52, 56, 62));         // gris foncé en bas
        p.fillRect(rect(), bg);

        // éclats décoratifs
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(210, 214, 220, 90));
        p.drawPolygon(QPolygonF({QPointF(0, H * 0.22), QPointF(W * 0.05, H * 0.245), QPointF(0, H * 0.26)}));
        p.drawPolygon(QPolygonF({QPointF(W, H * 0.10), QPointF(W * 0.93, H * 0.17), QPointF(W, H * 0.13)}));
        p.drawPolygon(QPolygonF({QPointF(0, H), QPointF(W * 0.08, H * 0.84), QPointF(W * 0.10, H)}));

        // slogan
        QFont tag = font();
        tag.setBold(true);
        tag.setPixelSize(16);
        tag.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        p.setFont(tag);
        p.setPen(QColor(255, 255, 255, 220));
        p.drawText(QRectF(0, 16, W, 30), Qt::AlignCenter, "VOS CIRCUITS. VOS PROJETS. VOS RÈGLES.");

        for (int i = 0; i < 3; ++i)
            paintCard(p, i);

        // pied de page
        QFont foot = font();
        foot.setBold(true);
        foot.setPixelSize(15);
        foot.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        p.setFont(foot);
        p.setPen(Qt::white);
        p.drawText(QRectF(0, H - 44, W, 24), Qt::AlignCenter, "SÉLECTIONNEZ UN MODULE");
        foot.setPixelSize(24);
        foot.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        p.setFont(foot);
        p.drawText(QRectF(0, H - 50, W - 28, 34), Qt::AlignRight | Qt::AlignVCenter, "VOLTIX");
    }

private:
    struct Info {
        int module;
        const char *title, *sub, *img, *glyph;
        QColor c1, c2;
        bool beta;
    };
    static constexpr int kCount = 3;
    inline static const Info kInfo[kCount] = {
                                               {ModElectronics, "ÉLECTRONIQUE", "Circuits, composants et simulation SPICE", "electronique",
                                                "Ω", QColor(110, 50, 200), QColor(20, 15, 70), false},
                                               {ModElectrotech, "ÉLECTROTECHNIQUE", "Puissance, commande, contacteurs (CEI 60617)",
                                                "electrotechnique", "3~", QColor(70, 100, 130), QColor(15, 25, 40), false},
                                               {ModElectromag, "ÉLECTROMAGNÉTIQUE", "Bobines et transformateurs", "electromagnetique",
                                                "Φ", QColor(210, 110, 30), QColor(20, 45, 110), true},
                                               };

    void choose(int i)
    {
        if (onChoose)
            onChoose(kInfo[i].module);
    }

    QRectF cardRect(int i) const
    {
        const qreal gap = qMax(16.0, width() * 0.02);
        qreal ch = height() * 0.60;
        qreal cw = ch * 0.62;
        const qreal maxw = (width() - 4 * gap) / 3.0;
        if (cw > maxw) {
            cw = maxw;
            ch = cw / 0.62;
        }
        const qreal total = 3 * cw + 2 * gap;
        const qreal x0 = (width() - total) / 2, y0 = (height() - ch) / 2 - 8;
        return QRectF(x0 + i * (cw + gap), y0, cw, ch);
    }

    int cardAt(const QPointF &pt) const
    {
        for (int i = 0; i < 3; ++i)
            if (cardRect(i).adjusted(-8, -8, 8, 8).contains(pt))
                return i;
        return -1;
    }

    void paintCard(QPainter &p, int i)
    {
        const Info &in = kInfo[i];
        QRectF r = cardRect(i);
        const bool act = (i == m_hover);
        if (act) {
            r.adjust(-10, -10, 10, 10);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 50));
            p.drawRoundedRect(r.adjusted(-8, -8, 8, 8), 8, 8);
        }

        QPainterPath clip;
        clip.addRoundedRect(r, 4, 4);
        p.save();
        p.setClipPath(clip);

        if (!m_img[i].isNull()) {
            const QPixmap s = m_img[i].scaled(r.size().toSize(), Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation);
            p.drawPixmap(r.topLeft() + QPointF((r.width() - s.width()) / 2,
                                               (r.height() - s.height()) / 2), s);
        } else {
            QLinearGradient g(r.topLeft(), r.bottomRight());
            g.setColorAt(0, in.c1);
            g.setColorAt(1, in.c2);
            p.fillRect(r, g);
            QFont gf = font();
            gf.setBold(true);
            gf.setPixelSize(int(r.width() * 0.62));
            p.setFont(gf);
            p.setPen(QColor(255, 255, 255, 60));
            p.drawText(r.adjusted(0, 0, 0, -r.height() * 0.3), Qt::AlignCenter,
                       QString::fromUtf8(in.glyph));
        }

        // voile sombre sous le texte
        QLinearGradient sh(0, r.top() + r.height() * 0.5, 0, r.bottom());
        sh.setColorAt(0, QColor(5, 15, 50, 0));
        sh.setColorAt(0.45, QColor(5, 15, 50, 220));
        sh.setColorAt(1, QColor(5, 15, 50, 245));
        p.fillRect(r, sh);

        const qreal ch = r.height();
        QFont tf = font();
        tf.setBold(true);
        int ts = int(r.width() * 0.12);
        tf.setPixelSize(ts);
        const QString title = QString::fromUtf8(in.title);
        while (ts > 9 && QFontMetrics(tf).horizontalAdvance(title) > r.width() - 24)
            tf.setPixelSize(--ts);
        p.setFont(tf);
        p.setPen(Qt::white);
        const QRectF tr(r.left() + 12, r.bottom() - 0.38 * ch, r.width() - 24, ts * 1.5);
        p.drawText(tr, Qt::AlignCenter, title);

        QFont sf = font();
        sf.setPixelSize(qMax(10, int(r.width() * 0.05)));
        p.setFont(sf);
        p.setPen(QColor(200, 220, 255));
        p.drawText(QRectF(r.left() + 14, tr.bottom() + 2, r.width() - 28, ch * 0.13),
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, QString::fromUtf8(in.sub));

        if (act) {                                   // bouton jaune incliné
            const qreal bw = r.width() * 0.6, bh = qMax(26.0, ch * 0.07), sk = bh * 0.3;
            const qreal bx = r.center().x() - bw / 2, by = r.bottom() - ch * 0.11;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 235, 0));
            p.drawPolygon(QPolygonF({QPointF(bx + sk, by), QPointF(bx + bw, by),
                                     QPointF(bx + bw - sk, by + bh), QPointF(bx, by + bh)}));
            QFont bf = font();
            bf.setBold(true);
            bf.setItalic(true);
            bf.setPixelSize(int(bh * 0.5));
            p.setFont(bf);
            p.setPen(QColor(20, 20, 20));
            p.drawText(QRectF(bx, by, bw, bh), Qt::AlignCenter, "COMMENCER");
        }
        p.restore();

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(act ? QColor(Qt::white) : QColor(190, 195, 202), act ? 4 : 3));
        p.drawRoundedRect(r, 4, 4);

        if (in.beta) {                               // étiquette BÊTA
            QFont bf = font();
            bf.setBold(true);
            bf.setPixelSize(12);
            p.setFont(bf);
            const QRectF b(r.left() + 10, r.top() + 10, 52, 20);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 140, 0));
            p.drawRoundedRect(b, 4, 4);
            p.setPen(Qt::white);
            p.drawText(b, Qt::AlignCenter, "BÊTA");
        }
    }

    QPixmap m_img[3];
    int m_hover = -1;
};