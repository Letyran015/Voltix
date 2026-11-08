#pragma once
#include <QColor>
#include <QPainter>
#include <QPolygonF>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <QtMath>
#include <cmath>

class PlotWidget : public QWidget
{
public:
    explicit PlotWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(600, 400);
    }

    void setData(const QVector<double> &t, const QList<QVector<double>> &series,
                 const QStringList &names)
    {
        m_t = t;
        m_s = series;
        m_n = names;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);

        if (m_t.size() < 2 || m_s.isEmpty()) {
            p.setPen(Qt::black);
            p.drawText(rect(), Qt::AlignCenter, "Pas de données");
            return;
        }

        const QRectF pr(80, 20, width() - 100, height() - 70);
        const double t0 = m_t.first(), t1 = m_t.last();
        double ymin = 1e300, ymax = -1e300;
        for (const auto &s : std::as_const(m_s))
            for (double v : s) {
                ymin = qMin(ymin, v);
                ymax = qMax(ymax, v);
            }
        if (ymax - ymin < 1e-12) {
            ymin -= 1;
            ymax += 1;
        } else {
            const double pad = 0.05 * (ymax - ymin);
            ymin -= pad;
            ymax += pad;
        }

        auto mx = [&](double t) { return pr.left() + (t - t0) / (t1 - t0) * pr.width(); };
        auto my = [&](double v) { return pr.bottom() - (v - ymin) / (ymax - ymin) * pr.height(); };

        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);

        // grille et graduations
        const double sx = niceStep(t1 - t0, 8);
        const double sy = niceStep(ymax - ymin, 6);
        p.setPen(QPen(QColor(220, 220, 220), 1));
        const double fx = std::ceil(t0 / sx) * sx;
        for (int i = 0;; ++i) {
            const double x = fx + i * sx;
            if (x > t1 + sx * 1e-6) break;
            p.setPen(QPen(QColor(220, 220, 220), 1));
            p.drawLine(QPointF(mx(x), pr.top()), QPointF(mx(x), pr.bottom()));
            p.setPen(Qt::black);
            p.drawText(QRectF(mx(x) - 40, pr.bottom() + 4, 80, 16), Qt::AlignHCenter, eng(x, "s"));
        }
        const double fy = std::ceil(ymin / sy) * sy;
        for (int i = 0;; ++i) {
            const double y = fy + i * sy;
            if (y > ymax + sy * 1e-6) break;
            p.setPen(QPen(QColor(220, 220, 220), 1));
            p.drawLine(QPointF(pr.left(), my(y)), QPointF(pr.right(), my(y)));
            p.setPen(Qt::black);
            p.drawText(QRectF(0, my(y) - 8, pr.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                       eng(y, "V"));
        }
        p.setPen(QPen(Qt::black, 1));
        p.drawRect(pr);

        // courbes
        static const QColor colors[] = {QColor(0, 90, 200), QColor(210, 40, 40), QColor(0, 150, 60),
                                        QColor(200, 120, 0), QColor(130, 50, 160)};
        p.save();
        p.setClipRect(pr);
        for (int k = 0; k < m_s.size(); ++k) {
            QPolygonF poly;
            for (int i = 0; i < m_t.size() && i < m_s[k].size(); ++i)
                poly << QPointF(mx(m_t[i]), my(m_s[k][i]));
            p.setPen(QPen(colors[k % 5], 2));
            p.drawPolyline(poly);
        }
        p.restore();

        // légende
        for (int k = 0; k < m_n.size(); ++k) {
            const double y = pr.top() + 14 + k * 16;
            p.setPen(QPen(colors[k % 5], 3));
            p.drawLine(QPointF(pr.right() - 90, y), QPointF(pr.right() - 70, y));
            p.setPen(Qt::black);
            p.drawText(QPointF(pr.right() - 64, y + 4), "V(" + m_n[k] + ")");
        }
    }

private:
    static double niceStep(double range, int target)
    {
        const double raw = range / target;
        const double mag = std::pow(10.0, std::floor(std::log10(raw)));
        const double f = raw / mag;
        const double n = f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10;
        return n * mag;
    }

    static QString eng(double v, const QString &unit)
    {
        if (std::fabs(v) < 1e-18)
            return "0 " + unit;
        static const char *pre[] = {"p", "n", "µ", "m", "", "k", "M", "G"};
        int e = int(std::floor(std::log10(std::fabs(v)) / 3.0));
        e = qBound(-4, e, 3);
        const double x = v / std::pow(10.0, 3 * e);
        return QString("%1 %2%3").arg(x, 0, 'g', 4).arg(QString::fromUtf8(pre[e + 4]), unit);
    }

    QVector<double> m_t;
    QList<QVector<double>> m_s;
    QStringList m_n;
};