#pragma once
#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>

struct ProjectInfo {
    QString paper = "A4";                 // "A4" ou "A3"
    QString company, address, title, author;
    QString revision = "A";
    QString created = QDate::currentDate().toString(Qt::ISODate);
    QString modified = QDate::currentDate().toString(Qt::ISODate);
};

struct Folio {
    QString title;
    QByteArray state;                     // contenu du folio (JSON)
    QList<QByteArray> undo, redo;
};