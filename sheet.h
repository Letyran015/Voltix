#pragma once
#include "project.h"

#include <QFontMetrics>
#include <QGraphicsItem>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

// 1 mm = 4 unités de scène : la grille de 20 unités vaut 5 mm
// La feuille se dessine selon le modèle de cadre du projet (project.h : SheetFrame) :
//   normal, cadre seul, cadre + cartouche, cadre + cartouche avec logo, modèle 1 simple,
//   en A4, A3 ou A2, paysage ou portrait.
class SheetItem : public QGraphicsItem
{
public:
    SheetItem()
    {
        setZValue(-100);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    static QSizeF paperSize(const ProjectInfo &pi)
    {
        QSizeF s = pi.paper == "A2" ? QSizeF(2376, 1680) : pi.paper == "A3" ? QSizeF(1680, 1188) : QSizeF(1188, 840);
        return pi.portrait ? s.transposed() : s;
    }

    // zone des repères (cadre intérieur), identique pour tous les modèles : le repérage
    // « folio - colonne » ne change donc pas quand on change de cadre
    static QRectF refRect(const ProjectInfo &pi)
    {
        return QRectF(QPointF(0, 0), paperSize(pi)).adjusted(44, 44, -44, -44);
    }
    static int columnCount(const ProjectInfo &pi) { return qMax(4, qRound(refRect(pi).width() / 80.0)); }
    static int rowCount(const ProjectInfo &pi) { return qMax(3, qRound(refRect(pi).height() / 80.0)); }
    static QString columnName(int c)
    {
        return c < 26 ? QString(QChar('A' + c)) : QString(QChar('A' + c / 26 - 1)) + QChar('A' + c % 26);
    }

    // Lettre de la colonne du cadre qui contient l'abscisse x (repérage « folio - colonne »)
    static QString columnOf(const ProjectInfo &pi, qreal x)
    {
        const QRectF inner = refRect(pi);
        const int cols = columnCount(pi);
        const int c = qBound(0, int((x - inner.left()) / (inner.width() / cols)), cols - 1);
        return columnName(c);
    }

    // zone utile au dessin (là où la grille est tracée)
    static QRectF drawRect(const ProjectInfo &pi)
    {
        const QRectF page(QPointF(0, 0), paperSize(pi));
        if (pi.frame == FrameNormal)
            return page.adjusted(20, 20, -20, -20);
        if (pi.frame == FrameSimple)
            return page.adjusted(28, 28, -28, -28 - 44);
        return refRect(pi);
    }

    void setInfo(const ProjectInfo &pi, int index, int count, const QString &folioTitle)
    {
        prepareGeometryChange();
        if (pi.logo != m_pi.logo)
            m_logo = pi.logo.isEmpty() ? QImage() : QImage::fromData(pi.logo);
        m_pi = pi;
        m_index = index;
        m_count = count;
        m_title = folioTitle;
        update();
    }
    void setShowGrid(bool on)
    {
        if (m_grid != on) { m_grid = on; update(); }
    }

    QRectF boundingRect() const override
    {
        return QRectF(QPointF(-2, -2), paperSize(m_pi) + QSizeF(4, 4));
    }
    QPainterPath shape() const override { return QPainterPath(); }   // jamais cliquable

    void paint(QPainter *p, const QStyleOptionGraphicsItem *opt, QWidget *) override
    {
        const QRectF page(QPointF(0, 0), paperSize(m_pi));
        const QRectF area = drawRect(m_pi);

        p->fillRect(page, Qt::white);

        if (m_grid) {
            const qreal lod = opt ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform())
                                  : 1.0;
            if (lod >= 0.3) {
                p->setPen(QPen(QColor(170, 170, 170), 0));
                const int x0 = (int(area.left()) / 20 + 1) * 20, y0 = (int(area.top()) / 20 + 1) * 20;
                for (int x = x0; x < area.right(); x += 20)
                    for (int y = y0; y < area.bottom(); y += 20)
                        p->drawPoint(x, y);
            }
        }

        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(120, 120, 120), 1));
        p->drawRect(page);

        switch (m_pi.frame) {
        case FrameNormal:
            break;
        case FrameSimple:
            paintSimple(p, page);
            break;
        case FrameCadre:
            paintCadre(p, page);
            break;
        case FrameLogo:
            paintCadre(p, page);
            paintCartouche(p, true);
            break;
        default:
            paintCadre(p, page);
            paintCartouche(p, false);
            break;
        }
    }

private:
    // ---- cadre double, repères de colonnes (lettres) et de lignes (chiffres) ----
    void paintCadre(QPainter *p, const QRectF &page) const
    {
        const QRectF outer = page.adjusted(20, 20, -20, -20);
        const QRectF inner = refRect(m_pi);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(Qt::black, 2));
        p->drawRect(inner);
        p->setPen(QPen(Qt::black, 1));
        p->drawRect(outer);

        const int cols = columnCount(m_pi), rows = rowCount(m_pi);
        const qreal cw = inner.width() / cols, rh = inner.height() / rows;
        for (int c = 0; c < cols; ++c) {
            const qreal x0 = inner.left() + c * cw;
            if (c > 0) {
                p->setPen(QPen(Qt::black, 1));
                p->drawLine(QPointF(x0, outer.top()), QPointF(x0, inner.top()));
                p->drawLine(QPointF(x0, inner.bottom()), QPointF(x0, outer.bottom()));
            }
            const QString l = columnName(c);
            text(p, QRectF(x0, outer.top(), cw, inner.top() - outer.top()), Qt::AlignCenter, l, 12);
            text(p, QRectF(x0, inner.bottom(), cw, outer.bottom() - inner.bottom()), Qt::AlignCenter, l, 12);
        }
        for (int r = 0; r < rows; ++r) {
            const qreal y0 = inner.top() + r * rh;
            if (r > 0) {
                p->setPen(QPen(Qt::black, 1));
                p->drawLine(QPointF(outer.left(), y0), QPointF(inner.left(), y0));
                p->drawLine(QPointF(inner.right(), y0), QPointF(outer.right(), y0));
            }
            const QString n = QString::number(r + 1);
            text(p, QRectF(outer.left(), y0, inner.left() - outer.left(), rh), Qt::AlignCenter, n, 12);
            text(p, QRectF(inner.right(), y0, outer.right() - inner.right(), rh), Qt::AlignCenter, n, 12);
        }
    }

    // ---- cartouche (en bas à droite du cadre), avec ou sans case de logo ----
    void paintCartouche(QPainter *p, bool withLogo) const
    {
        const QRectF inner = refRect(m_pi);
        const qreal logoW = withLogo ? 130 : 0;
        const qreal wTot = qMin(inner.width(), 620.0 + logoW);
        const QRectF tb(inner.right() - wTot, inner.bottom() - 112, wTot, 112);
        p->fillRect(tb, Qt::white);
        p->setPen(QPen(Qt::black, 2));
        p->drawRect(tb);
        p->setPen(QPen(Qt::black, 1));
        const qreal xA = tb.left() + logoW;                      // fin de la case du logo
        const qreal xB = xA + 180, xC = xA + 420, xD = xA + 560;
        if (withLogo)
            p->drawLine(QPointF(xA, tb.top()), QPointF(xA, tb.bottom()));
        p->drawLine(QPointF(xB, tb.top()), QPointF(xB, tb.bottom()));
        p->drawLine(QPointF(xC, tb.top()), QPointF(xC, tb.bottom()));
        p->drawLine(QPointF(xD, tb.top()), QPointF(xD, tb.bottom()));
        p->drawLine(QPointF(xB, tb.center().y()), QPointF(xC, tb.center().y()));
        for (int i = 1; i < 4; ++i)
            p->drawLine(QPointF(xC, tb.top() + i * 28), QPointF(xD, tb.top() + i * 28));

        if (withLogo) {
            const QRectF box = QRectF(tb.left(), tb.top(), logoW, tb.height()).adjusted(8, 8, -8, -8);
            if (!m_logo.isNull()) {
                const QSizeF s = QSizeF(m_logo.size()).scaled(box.size(), Qt::KeepAspectRatio);
                const QRectF r(box.center().x() - s.width() / 2, box.center().y() - s.height() / 2, s.width(), s.height());
                p->setRenderHint(QPainter::SmoothPixmapTransform);
                p->drawImage(r, m_logo);
            } else {
                p->setPen(QPen(QColor(150, 150, 150), 1, Qt::DashLine));
                p->drawRect(box);
                text(p, box, Qt::AlignCenter, "LOGO", 16, true, false, QColor(150, 150, 150));
            }
        }

        text(p, QRectF(xA + 8, tb.top() + 6, 164, 26), Qt::AlignLeft | Qt::AlignVCenter,
             m_pi.company, 14, true);
        text(p, QRectF(xA + 8, tb.top() + 36, 164, 70), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
             m_pi.address, 11, false, false);
        text(p, QRectF(xB + 8, tb.top() + 4, 224, 48), Qt::AlignLeft | Qt::AlignVCenter,
             m_title, 16, true);
        text(p, QRectF(xB + 8, tb.center().y() + 4, 224, 48), Qt::AlignLeft | Qt::AlignVCenter,
             m_pi.title, 12);
        const QStringList rowsTxt = {
            QString("Dessiné le : %1").arg(fmt(m_pi.created)),
            QString("Modifié le : %1").arg(fmt(m_pi.modified)),
            QString("Par : %1").arg(m_pi.author),
            QString("Indice : %1").arg(m_pi.revision)};
        for (int i = 0; i < 4; ++i)
            text(p, QRectF(xC + 6, tb.top() + i * 28, 128, 28), Qt::AlignLeft | Qt::AlignVCenter,
                 rowsTxt[i], 11);
        text(p, QRectF(xD, tb.top() + 6, 60, 18), Qt::AlignCenter, "Folio", 10);
        text(p, QRectF(xD, tb.top() + 28, 60, 38), Qt::AlignCenter,
             QString("%1").arg(m_index + 1, 2, 10, QChar('0')), 20, true);
        text(p, QRectF(xD, tb.top() + 66, 60, 36), Qt::AlignCenter,
             QString("/ %1").arg(m_count, 2, 10, QChar('0')), 14);
    }

    // ---- modèle 1 - simple : un filet et un bandeau de titre sur toute la largeur ----
    void paintSimple(QPainter *p, const QRectF &page) const
    {
        const QRectF fr = page.adjusted(28, 28, -28, -28);
        const qreal yS = fr.bottom() - 44;
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(Qt::black, 2));
        p->drawRect(fr);
        p->drawLine(QPointF(fr.left(), yS), QPointF(fr.right(), yS));
        const qreal w = fr.width();
        const qreal x1 = fr.left() + w * 0.24, x2 = fr.left() + w * 0.62, x3 = fr.left() + w * 0.76, x4 = fr.left() + w * 0.89;
        p->setPen(QPen(Qt::black, 1));
        for (qreal x : {x1, x2, x3, x4})
            p->drawLine(QPointF(x, yS), QPointF(x, fr.bottom()));
        const int f = Qt::AlignLeft | Qt::AlignVCenter;
        text(p, QRectF(fr.left() + 8, yS, x1 - fr.left() - 8, 44), f, m_pi.company, 13, true);
        text(p, QRectF(x1 + 8, yS + 2, x2 - x1 - 8, 22), f, m_title, 14, true);
        text(p, QRectF(x1 + 8, yS + 22, x2 - x1 - 8, 20), f, m_pi.title, 11);
        text(p, QRectF(x2 + 8, yS, x3 - x2 - 8, 44), f, m_pi.author, 11);
        text(p, QRectF(x3 + 8, yS + 2, x4 - x3 - 8, 22), f, fmt(m_pi.modified), 11);
        text(p, QRectF(x3 + 8, yS + 22, x4 - x3 - 8, 20), f, QString("Indice %1").arg(m_pi.revision), 11);
        text(p, QRectF(x4, yS, fr.right() - x4, 44), Qt::AlignCenter,
             QString("Folio %1 / %2").arg(m_index + 1).arg(m_count), 13, true);
    }

    static QString fmt(const QString &iso)
    {
        const QDate d = QDate::fromString(iso, Qt::ISODate);
        return d.isValid() ? d.toString("dd/MM/yyyy") : QString();
    }

    static void text(QPainter *p, const QRectF &r, int flags, const QString &s, int px,
                     bool bold = false, bool elide = true, const QColor &col = Qt::black)
    {
        QFont f = p->font();
        f.setPixelSize(px);
        f.setBold(bold);
        p->setFont(f);
        p->setPen(col);
        const QString t = (elide && !(flags & Qt::TextWordWrap))
                              ? QFontMetrics(f).elidedText(s, Qt::ElideRight, int(r.width()) - 6)
                              : s;
        p->drawText(r, flags, t);
    }

    ProjectInfo m_pi;
    QImage m_logo;
    int m_index = 0, m_count = 1;
    QString m_title;
    bool m_grid = true;
};
