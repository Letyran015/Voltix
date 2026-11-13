#pragma once
#include "project.h"

#include <QFontMetrics>
#include <QGraphicsItem>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

// 1 mm = 4 unités de scène : la grille de 20 unités vaut 5 mm
class SheetItem : public QGraphicsItem
{
public:
    SheetItem()
    {
        setZValue(-100);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    static QSizeF paperSize(const QString &paper)
    {
        return paper == "A3" ? QSizeF(1680, 1188) : QSizeF(1188, 840);
    }

    void setInfo(const ProjectInfo &pi, int index, int count, const QString &folioTitle)
    {
        prepareGeometryChange();
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
        return QRectF(QPointF(-2, -2), paperSize(m_pi.paper) + QSizeF(4, 4));
    }
    QPainterPath shape() const override { return QPainterPath(); }   // jamais cliquable

    void paint(QPainter *p, const QStyleOptionGraphicsItem *opt, QWidget *) override
    {
        const QSizeF sz = paperSize(m_pi.paper);
        const QRectF page(QPointF(0, 0), sz);
        const QRectF outer = page.adjusted(20, 20, -20, -20);
        const QRectF inner = page.adjusted(44, 44, -44, -44);

        p->fillRect(page, Qt::white);

        if (m_grid) {
            const qreal lod = opt ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(p->worldTransform())
                                  : 1.0;
            if (lod >= 0.3) {
                p->setPen(QPen(QColor(170, 170, 170), 0));
                for (int x = 60; x < inner.right(); x += 20)
                    for (int y = 60; y < inner.bottom(); y += 20)
                        p->drawPoint(x, y);
            }
        }

        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(120, 120, 120), 1));
        p->drawRect(page);
        p->setPen(QPen(Qt::black, 2));
        p->drawRect(inner);
        p->setPen(QPen(Qt::black, 1));
        p->drawRect(outer);

        // repères de colonnes (lettres) et de lignes (chiffres)
        const int cols = qMax(4, qRound(inner.width() / 80.0));
        const int rows = qMax(3, qRound(inner.height() / 80.0));
        const qreal cw = inner.width() / cols, rh = inner.height() / rows;
        for (int c = 0; c < cols; ++c) {
            const qreal x0 = inner.left() + c * cw;
            if (c > 0) {
                p->setPen(QPen(Qt::black, 1));
                p->drawLine(QPointF(x0, outer.top()), QPointF(x0, inner.top()));
                p->drawLine(QPointF(x0, inner.bottom()), QPointF(x0, outer.bottom()));
            }
            const QString l(QChar('A' + (c % 26)));
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

        // cartouche
        const QRectF tb(inner.right() - 620, inner.bottom() - 112, 620, 112);
        p->fillRect(tb, Qt::white);
        p->setPen(QPen(Qt::black, 2));
        p->drawRect(tb);
        p->setPen(QPen(Qt::black, 1));
        const qreal xB = tb.left() + 180, xC = tb.left() + 420, xD = tb.left() + 560;
        p->drawLine(QPointF(xB, tb.top()), QPointF(xB, tb.bottom()));
        p->drawLine(QPointF(xC, tb.top()), QPointF(xC, tb.bottom()));
        p->drawLine(QPointF(xD, tb.top()), QPointF(xD, tb.bottom()));
        p->drawLine(QPointF(xB, tb.center().y()), QPointF(xC, tb.center().y()));
        for (int i = 1; i < 4; ++i)
            p->drawLine(QPointF(xC, tb.top() + i * 28), QPointF(xD, tb.top() + i * 28));

        text(p, QRectF(tb.left() + 8, tb.top() + 6, 164, 26), Qt::AlignLeft | Qt::AlignVCenter,
             m_pi.company, 14, true);
        text(p, QRectF(tb.left() + 8, tb.top() + 36, 164, 70), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
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

private:
    static QString fmt(const QString &iso)
    {
        const QDate d = QDate::fromString(iso, Qt::ISODate);
        return d.isValid() ? d.toString("dd/MM/yyyy") : QString();
    }

    static void text(QPainter *p, const QRectF &r, int flags, const QString &s, int px,
                     bool bold = false, bool elide = true)
    {
        QFont f = p->font();
        f.setPixelSize(px);
        f.setBold(bold);
        p->setFont(f);
        p->setPen(Qt::black);
        const QString t = (elide && !(flags & Qt::TextWordWrap))
                              ? QFontMetrics(f).elidedText(s, Qt::ElideRight, int(r.width()) - 6)
                              : s;
        p->drawText(r, flags, t);
    }

    ProjectInfo m_pi;
    int m_index = 0, m_count = 1;
    QString m_title;
    bool m_grid = true;
};