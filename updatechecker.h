#pragma once
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QWidget>

#ifndef VOLTIX_VERSION
#define VOLTIX_VERSION "0.0.0-dev"
#endif

// Informateur de mise à jour : il ne télécharge ni n'exécute rien,
// il ouvre seulement la page des versions dans le navigateur.
class UpdateChecker : public QObject
{
public:
    explicit UpdateChecker(QWidget *parent) : QObject(parent), m_parent(parent), m_nam(this) {}

    static QString currentVersion() { return QStringLiteral(VOLTIX_VERSION); }

    // manual = true : l'utilisateur a demandé la vérification (on affiche toujours un résultat)
    // manual = false : vérification silencieuse, au plus une fois par jour
    void check(bool manual)
    {
        QSettings s;
        if (!manual) {
            if (!s.value("update/auto", true).toBool())
                return;
            if (currentVersion().startsWith("0.0.0"))      // version de développement
                return;
            const QDateTime last = s.value("update/lastCheck").toDateTime();
            if (last.isValid() && last.secsTo(QDateTime::currentDateTime()) < 24 * 3600)
                return;
        }
        if (m_busy)
            return;
        m_busy = true;

        QNetworkRequest req{QUrl(QString::fromLatin1(kApi))};
        req.setHeader(QNetworkRequest::UserAgentHeader, "Voltix/" + currentVersion());
        req.setRawHeader("Accept", "application/vnd.github+json");
        req.setTransferTimeout(10000);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, manual] { handle(reply, manual); });
    }

private:
    static constexpr const char *kApi =
        "https://api.github.com/repos/Letyran015/Voltix/releases/latest";
    static constexpr const char *kPage =
        "https://github.com/Letyran015/Voltix/releases/latest";

    static QList<int> parts(QString v)
    {
        v = v.trimmed();
        if (v.startsWith('v') || v.startsWith('V'))
            v.remove(0, 1);
        int end = 0;
        while (end < v.size() && (v[end].isDigit() || v[end] == '.'))
            ++end;
        QList<int> out;
        const QStringList sp = v.left(end).split('.', Qt::SkipEmptyParts);
        for (const QString &p : sp)
            out << p.toInt();
        return out;
    }

    static bool isNewer(const QString &a, const QString &b)
    {
        QList<int> x = parts(a), y = parts(b);
        const int n = qMax(qMax(x.size(), y.size()), 3);
        while (x.size() < n) x << 0;
        while (y.size() < n) y << 0;
        for (int i = 0; i < n; ++i)
            if (x[i] != y[i])
                return x[i] > y[i];
        return false;
    }

    void handle(QNetworkReply *reply, bool manual)
    {
        reply->deleteLater();
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            if (manual)
                QMessageBox::warning(m_parent, "Mises à jour",
                                     "Impossible de vérifier les mises à jour.\n\n"
                                         + reply->errorString());
            return;
        }

        const QByteArray data = reply->read(1 << 20);          // 1 Mo maximum
        const QJsonObject o = QJsonDocument::fromJson(data).object();
        const QString tag = o.value("tag_name").toString().left(64);
        const QString notes = o.value("body").toString().left(600);

        QSettings s;
        s.setValue("update/lastCheck", QDateTime::currentDateTime());

        if (tag.isEmpty()) {
            if (manual)
                QMessageBox::warning(m_parent, "Mises à jour",
                                     "Réponse inattendue du serveur.");
            return;
        }
        if (!isNewer(tag, currentVersion())) {
            if (manual)
                QMessageBox::information(m_parent, "Mises à jour",
                                         "Voltix est à jour (version " + currentVersion() + ").");
            return;
        }
        if (!manual && s.value("update/skip").toString() == tag)
            return;

        QMessageBox box(m_parent);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle("Mise à jour disponible");
        box.setTextFormat(Qt::PlainText);                       // le texte vient du réseau
        box.setText(QString("La version %1 de Voltix est disponible (vous avez la %2).")
                        .arg(tag, currentVersion()));
        if (!notes.isEmpty())
            box.setInformativeText(notes);
        QPushButton *open = box.addButton("Télécharger", QMessageBox::AcceptRole);
        QPushButton *skip = box.addButton("Ignorer cette version", QMessageBox::DestructiveRole);
        box.addButton("Plus tard", QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == open)
            QDesktopServices::openUrl(QUrl(QString::fromLatin1(kPage)));
        else if (box.clickedButton() == skip)
            s.setValue("update/skip", tag);
    }

    QWidget *m_parent;
    QNetworkAccessManager m_nam;
    bool m_busy = false;
};