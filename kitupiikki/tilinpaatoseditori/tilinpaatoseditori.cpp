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

#include <QDesktopServices>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QMessageBox>

#include "tilinpaatoseditori.h"
#include "tilinpaatostulostaja.h"
#include "db/kirjanpito.h"
#include "tpaloitus.h"

TilinpaatosEditori::TilinpaatosEditori(Tilikausi tilikausi)
    : QMainWindow(),
      tilikausi_(tilikausi),
      printer_( QPrinter::HighResolution)
{
    editori_ = new MRichTextEdit;
    setCentralWidget( editori_);
    setWindowTitle( tr("Tilinpäätöksen liitetiedot %1").arg(tilikausi.kausivaliTekstina()));

    setWindowIcon( QIcon(":/pic/Possu64.png"));

    printer_.setPageMargins( 25,10,10,10, QPrinter::Millimeter);

    luoAktiot();
    luoPalkit();

    setAttribute(Qt::WA_DeleteOnClose);

    lataa();
}

void TilinpaatosEditori::esikatsele()
{
    QString teksti = raportit_ + "\n" + editori_->toHtml();
    TilinpaatosTulostaja::tulostaTilinpaatos( tilikausi_, teksti , &printer_);
    kp()->tilikaudet()->tallennaTilinpaatosteksti( kp()->tilikaudet()->indeksiPaivalle(tilikausi_.paattyy()), teksti );

    QDesktopServices::openUrl( QUrl::fromLocalFile( kp()->hakemisto().absoluteFilePath("arkisto/" + tilikausi_.arkistoHakemistoNimi() + "/tilinpaatos.pdf") ));
}

void TilinpaatosEditori::luoAktiot()
{
    esikatseleAction_ = new QAction( QIcon(":/pic/print.png"), tr("Tallenna ja esikatsele"), this);
    connect( esikatseleAction_, SIGNAL(triggered(bool)), this, SLOT(esikatsele()));
    vahvistaAction_ = new QAction( QIcon(":/pic/ok.png"), tr("Valmis"), this);
    connect( vahvistaAction_, SIGNAL(triggered(bool)), this, SLOT(valmis()));
    aloitaUudelleenAktio_ = new QAction( QIcon(":/pic/uusitiedosto.png"), tr("Aloita uudelleen"), this);
    connect( aloitaUudelleenAktio_, SIGNAL(triggered(bool)), this, SLOT(aloitaAlusta()));
    ohjeAktio_ = new QAction( QIcon(":/pic/ohje.png"), tr("Ohje"), this);
    connect( ohjeAktio_, SIGNAL(triggered(bool)), this, SLOT(ohje()));
}

void TilinpaatosEditori::luoPalkit()
{
    tilinpaatosTb_ = addToolBar( tr("&Tilinpäätös"));
    tilinpaatosTb_->addAction( esikatseleAction_ );
    tilinpaatosTb_->addAction( aloitaUudelleenAktio_ );
    tilinpaatosTb_->addAction( vahvistaAction_ );
    tilinpaatosTb_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
}

void TilinpaatosEditori::uusiTp()
{
    QStringList pohja = kp()->asetukset()->lista("TilinpaatosPohja");
    QStringList valinnat = kp()->asetukset()->lista("TilinpaatosValinnat");
    QRegularExpression tunnisteRe("#(?<tunniste>\\w+).*");
    tunnisteRe.setPatternOptions(QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpression raporttiRe("@(?<raportti>.+)!(?<otsikko>.+)@");

    QString teksti;
    raportit_.clear();

    bool tulosta = true;

    foreach (QString rivi, pohja)
    {
        if( rivi.startsWith('#') )
        {
            QRegularExpressionMatch tmats = tunnisteRe.match(rivi);
            if( tmats.hasMatch())
            {
                QString tunniste = tmats.captured(1);
                tulosta = valinnat.contains(tunniste);
            }
            else
                tulosta = true;
        }
        else if( rivi.startsWith('@') && tulosta)
        {
            // Näillä tulostetaan erityisiä kenttiä
            if( rivi == "@sha@")
                teksti.append( tilikausi_.json()->str("ArkistoSHA"));
            else
            {
                QRegularExpressionMatch rmats = raporttiRe.match(rivi);
                if( rmats.hasMatch())
                {
                    raportit_.append( rivi + " " );
                }
            }
        }
        else if( tulosta )
            teksti.append(rivi);
    }
    editori_->setText(teksti);
}

void TilinpaatosEditori::lataa()
{
    QString data = tilikausi_.json()->str("TilinpaatosTeksti");
    if( data.isEmpty())
        aloitaAlusta();
    else
    {
        raportit_ = data.left( data.indexOf("\n"));
        editori_->setText( data.mid(data.indexOf("\n")+1));
    }
}

void TilinpaatosEditori::aloitaAlusta()
{
    TpAloitus tpaloitus(tilikausi_);
    if( tpaloitus.exec() == QDialog::Accepted)
    {
        uusiTp();   // Aloitetaan alusta
    }
    else
    {
        hide();
        close();
    }
}

void TilinpaatosEditori::valmis()
{
    if( QMessageBox::question(this, tr("Vahvista tilinpäätös"),
                              tr("Onko tilinpäätös vahvistettu lopulliseksi?\n"
                                 "Vahvistettua tilinpäätöstä ei voi enää muokata.")) != QMessageBox::Yes)
        return;

    kp()->tilikaudet()->vaihdaTilinpaatostila( kp()->tilikaudet()->indeksiPaivalle(tilikausi_.paattyy()) ,  Tilikausi::VAHVISTETTU);
    emit kp()->onni("Tilinpäätös merkitty valmiiksi");
    close();
}

void TilinpaatosEditori::ohje()
{
    QDesktopServices::openUrl(QUrl("https://artoh.github.io/kitupiikki/tilinpaatos/"));
}
