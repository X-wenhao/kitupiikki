/*
   Copyright (C) 2017 Arto Hyvättinen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef JSONKENTTA_H
#define JSONKENTTA_H

#include <QDate>
#include <QMap>
#include <QVariant>

/**
 * @brief json-muotoisten kenttien käsittely
 *
 * Laajennettavuutta ja yksinkertaisempaa tietokantaa silmällä pitäen käytetään
 * json-muotoisia kenttiä, joita käsitellään tämän luokan kautta
 *
 */
class JsonKentta
{
public:
    JsonKentta();

    void set(const QString& avain, const QString& arvo);
    void set(const QString& avain, const QDate& pvm);
    void set(const QString& avain, int arvo);
    void unset(const QString &avain);

    QString str(const QString& avain);
    QDate date(const QString& avain);
    int luku(const QString& avain);

    QByteArray toJson();
    void fromJson(const QByteArray& json);


protected:
    QVariantMap map_;
};

#endif // JSONKENTTA_H
