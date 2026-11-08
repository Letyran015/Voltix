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
        setMinimumSize(600, 280);
    }

    // unités des axes ; logX : axe X logarithmique ; engY : préfixes (m, k, µ...) sur Y
    void setAxes(const QString &xUnit, const QString &yUnit, bool logX = false, bool engY = true)
    {
        m_xu = xUnit; m_yu = yUnit; m_logX = logX; m_engY = engY;
        update();
    }

    void setData(const QVector<double> &x, const QList<QVector<double>> &series,
                 const QStringList &names)
    {
        m_x = x;
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

        if (m_x.size() < 2 || m_s.isEmpty() || (m_logX && m_x.first() <= 0)) {
            p.setPen(Qt::black);
            p.drawText(rect(), Qt::AlignCenter, "Pas de données");
            return;
        }

        const QRectF pr(80, 20, width() - 100, height() - 60);
        const double a0 = X(m_x.first()), a1 = X(m_x.last());
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

        auto mx = [&](double x) { return pr.left() + (X(x) - a0) / (a1 - a0) * pr.width(); };
        auto my = [&](double v) { return pr.bottom() - (v - ymin) / (ymax - ymin) * pr.height(); };

        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);

        const QColor grid(220, 220, 220), gridMinor(238, 238, 238);

        // graduations X
        if (m_logX) {
            for (int e = qFloor(a0); e <= qCeil(a1); ++e)
                for (int m = 1; m <= 9; ++m) {
                    const double x = m * std::pow(10.0, e);
                    const double lx = std::log10(x);
                    if (lx < a0 - 1e-9 || lx > a1 + 1e-9) continue;
                    p.setPen(QPen(m == 1 ? grid : gridMinor, 1));
                    p.drawLine(QPointF(mx(x), pr.top()), QPointF(mx(x), pr.bottom()));
                    if (m == 1) {
                        p.setPen(Qt::black);
                        p.drawText(QRectF(mx(x) - 40, pr.bottom() + 4, 80, 16), Qt::AlignHCenter,
                                   eng(x, m_xu));
                    }
                }
        } else {
            const double sx = niceStep(a1 - a0, 8);
            const double fx = std::ceil(a0 / sx) * sx;
            for (int i = 0;; ++i) {
                const double x = fx + i * sx;
                if (x > a1 + sx * 1e-6) break;
                p.setPen(QPen(grid, 1));
                p.drawLine(QPointF(mx(x), pr.top()), QPointF(mx(x), pr.bottom()));
                p.setPen(Qt::black);
                p.drawText(QRectF(mx(x) - 40, pr.bottom() + 4, 80, 16), Qt::AlignHCenter,
                           eng(x, m_xu));
            }
        }

        // graduations Y
        const double sy = niceStep(ymax - ymin, 6);
        const double fy = std::ceil(ymin / sy) * sy;
        for (int i = 0;; ++i) {
            const double y = fy + i * sy;
            if (y > ymax + sy * 1e-6) break;
            p.setPen(QPen(grid, 1));
            p.drawLine(QPointF(pr.left(), my(y)), QPointF(pr.right(), my(y)));
            p.setPen(Qt::black);
            p.drawText(QRectF(0, my(y) - 8, pr.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                       m_engY ? eng(y, m_yu) : plain(y, m_yu));
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
            for (int i = 0; i < m_x.size() && i < m_s[k].size(); ++i) {
                if (m_logX && m_x[i] <= 0) continue;
                poly << QPointF(mx(m_x[i]), my(m_s[k][i]));
            }
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
            p.drawText(QPointF(pr.right() - 64, y + 4), m_n[k]);
        }
    }

private:
    double X(double x) const { return m_logX ? std::log10(x) : x; }

    static double niceStep(double range, int target)
    {
        const double raw = range / target;
        const double mag = std::pow(10.0, std::floor(std::log10(raw)));
        const double f = raw / mag;
        const double n = f < 1.5 ? 1 : f < 3 ? 2 : f < 7 ? 5 : 10;
        return n * mag;
    }

    static QString plain(double v, const QString &unit)
    {
        return QString("%1 %2").arg(v, 0, 'g', 4).arg(unit).trimmed();
    }

    static QString eng(double v, const QString &unit)
    {
        if (std::fabs(v) < 1e-18)
            return QString("0 %1").arg(unit).trimmed();
        static const char *pre[] = {"p", "n", "µ", "m", "", "k", "M", "G"};
        int e = int(std::floor(std::log10(std::fabs(v)) / 3.0));
        e = qBound(-4, e, 3);
        const double x = v / std::pow(10.0, 3 * e);
        return QString("%1 %2%3").arg(x, 0, 'g', 4).arg(QString::fromUtf8(pre[e + 4]), unit).trimmed();
    }

    QVector<double> m_x;
    QList<QVector<double>> m_s;
    QStringList m_n;
    QString m_xu = "s", m_yu = "V";
    bool m_logX = false, m_engY = true;
};