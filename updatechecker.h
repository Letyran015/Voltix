#pragma once
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
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

// Mise à jour de Voltix.
//  - Windows : l'installateur de la nouvelle version est téléchargé depuis les versions GitHub du projet
//    (HTTPS uniquement, hôtes GitHub uniquement), vérifié (SHA-256 publié par GitHub, taille, en-tête
//    d'exécutable), puis lancé en mode silencieux par-dessus l'installation actuelle. Voltix se ferme
//    et se relance tout seul à la fin.
//  - Autres cas (pas d'empreinte SHA-256 publiée, autre système) : on ouvre la page des versions.
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

        // installateur Windows publie avec la version (nom contenant « setup », extension .exe)
        QUrl setupUrl;
        qint64 setupSize = 0;
        QString setupSha;
        for (const QJsonValue &av : o.value("assets").toArray()) {
            const QJsonObject a = av.toObject();
            const QString name = a.value("name").toString();
            if (!name.endsWith(".exe", Qt::CaseInsensitive) || !name.contains("setup", Qt::CaseInsensitive))
                continue;
            setupUrl = QUrl(a.value("browser_download_url").toString());
            setupSize = a.value("size").toVariant().toLongLong();
            const QString d = a.value("digest").toString();
            if (d.startsWith("sha256:", Qt::CaseInsensitive))
                setupSha = d.mid(7).toLower();
            break;
        }
        bool canAuto = false;
#ifdef Q_OS_WIN
        canAuto = setupUrl.isValid() && allowedUrl(setupUrl) && setupSha.size() == 64 && setupSize > 0
                  && setupSize < (qint64(1) << 30);
#endif

        QMessageBox box(m_parent);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle("Mise à jour disponible");
        box.setTextFormat(Qt::PlainText);                       // le texte vient du réseau
        box.setText(QString("La version %1 de Voltix est disponible (vous avez la %2).")
                        .arg(tag, currentVersion()));
        if (!notes.isEmpty())
            box.setInformativeText(notes);
        QPushButton *now = canAuto ? box.addButton("Mettre à jour maintenant", QMessageBox::AcceptRole) : nullptr;
        QPushButton *open = box.addButton(canAuto ? "Voir sur GitHub" : "Télécharger", QMessageBox::ActionRole);
        QPushButton *skip = box.addButton("Ignorer cette version", QMessageBox::DestructiveRole);
        box.addButton("Plus tard", QMessageBox::RejectRole);
        box.exec();

        if (now && box.clickedButton() == now)
            startDownload(setupUrl, setupSize, setupSha, tag);
        else if (box.clickedButton() == open)
            QDesktopServices::openUrl(QUrl(QString::fromLatin1(kPage)));
        else if (box.clickedButton() == skip)
            s.setValue("update/skip", tag);
    }

    // seuls les serveurs de GitHub sont acceptes, en HTTPS
    static bool allowedUrl(const QUrl &u)
    {
        if (u.scheme() != "https")
            return false;
        const QString h = u.host().toLower();
        return h == "github.com" || h.endsWith(".github.com") || h.endsWith(".githubusercontent.com");
    }

    void startDownload(const QUrl &url, qint64 expectedSize, const QString &sha, const QString &tag)
    {
        QString safeTag = tag;
        safeTag.remove(QRegularExpression("[^A-Za-z0-9._-]"));
        const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("Voltix-update");
        QDir().mkpath(dir);
        const QString path = QDir(dir).filePath("Voltix-Setup-" + safeTag + ".exe");
        QFile::remove(path);

        auto *file = new QFile(path, this);
        if (!file->open(QIODevice::WriteOnly)) {
            QMessageBox::warning(m_parent, "Mises à jour", "Impossible d'écrire le fichier d'installation :\n" + path);
            file->deleteLater();
            return;
        }

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, "Voltix/" + currentVersion());
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(30000);
        QNetworkReply *reply = m_nam.get(req);

        auto *hash = new QCryptographicHash(QCryptographicHash::Sha256);
        auto *progress = new QProgressDialog("Téléchargement de Voltix " + tag + "…", "Annuler", 0, 100, m_parent);
        progress->setWindowTitle("Mise à jour");
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setAutoClose(false);
        progress->setAutoReset(false);
        progress->setValue(0);

        connect(reply, &QNetworkReply::readyRead, this, [reply, file, hash, expectedSize, progress] {
            const QByteArray chunk = reply->readAll();
            if (file->size() + chunk.size() > expectedSize + 1024) {   // plus gros que prevu : on coupe
                reply->abort();
                return;
            }
            file->write(chunk);
            hash->addData(chunk);
        });
        connect(reply, &QNetworkReply::downloadProgress, this, [progress, expectedSize](qint64 got, qint64) {
            if (expectedSize > 0) progress->setValue(int(qMin<qint64>(99, got * 100 / expectedSize)));
        });
        connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, file, hash, progress, path, expectedSize, sha] {
            const QByteArray rest = reply->readAll();
            if (!rest.isEmpty()) { file->write(rest); hash->addData(rest); }
            file->close();
            const qint64 size = file->size();
            const QByteArray digest = hash->result().toHex();
            const bool netOk = reply->error() == QNetworkReply::NoError;
            const bool hostOk = allowedUrl(reply->url());
            const QString err = reply->errorString();
            const bool cancelled = reply->error() == QNetworkReply::OperationCanceledError;
            reply->deleteLater(); file->deleteLater(); delete hash;
            progress->close(); progress->deleteLater();

            auto fail = [&](const QString &why) {
                QFile::remove(path);
                if (!cancelled)
                    QMessageBox::warning(m_parent, "Mises à jour",
                                         "La mise à jour n'a pas pu être installée automatiquement :\n" + why +
                                         "\n\nVous pouvez télécharger la nouvelle version depuis GitHub.");
            };
            if (!netOk)                         { fail(err); return; }
            if (!hostOk)                        { fail("serveur de téléchargement non autorisé"); return; }
            if (size != expectedSize)           { fail("taille inattendue du fichier téléchargé"); return; }
            if (QString::fromLatin1(digest) != sha) { fail("l'empreinte SHA-256 du fichier ne correspond pas"); return; }
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly) || f.read(2) != "MZ") { fail("le fichier n'est pas un installateur Windows"); return; }
            f.close();

            const auto ans = QMessageBox::question(m_parent, "Mise à jour prête",
                "La mise à jour est téléchargée et vérifiée.\n\n"
                "Voltix va se fermer, s'installer par-dessus la version actuelle, puis se relancer.\n"
                "Pensez à enregistrer votre travail. Continuer ?");
            if (ans != QMessageBox::Yes)
                return;
            launchInstaller(path);
        });
    }

    void launchInstaller(const QString &path)
    {
        // laisse chaque fenetre proposer d'enregistrer ; si l'utilisateur annule, on n'installe pas
        QApplication::closeAllWindows();
        for (QWidget *w : QApplication::topLevelWidgets())
            if (w->isVisible() && w->windowType() == Qt::Window) {
                QMessageBox::information(m_parent, "Mises à jour",
                                         "Mise à jour annulée : l'installateur reste disponible ici :\n" + path);
                return;
            }
        if (!QProcess::startDetached(path, {"/SILENT", "/CLOSEAPPLICATIONS", "/NORESTART"})) {
            QMessageBox::warning(nullptr, "Mises à jour", "Impossible de lancer l'installateur :\n" + path);
            return;
        }
        QApplication::quit();
    }

    QWidget *m_parent;
    QNetworkAccessManager m_nam;
    bool m_busy = false;
};