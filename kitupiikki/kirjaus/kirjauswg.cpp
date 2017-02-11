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

#include "kirjauswg.h"
#include "tilidelegaatti.h"
#include "eurodelegaatti.h"
#include "pvmdelegaatti.h"

#include "db/kirjanpito.h"

#include <QDebug>
#include <QSqlQuery>
#include <QMessageBox>
#include <QIntValidator>

#include <QSortFilterProxyModel>

KirjausWg::KirjausWg(TositeModel *tositeModel) : QWidget(), model(tositeModel), tositeId(0)
{


    ui = new Ui::KirjausWg();
    ui->setupUi(this);

    ui->viennitView->setModel( model->vientiModel() );

    connect( model->vientiModel() , SIGNAL(siirryRuutuun(QModelIndex)), ui->viennitView, SLOT(setCurrentIndex(QModelIndex)));
    connect( model->vientiModel(), SIGNAL(siirryRuutuun(QModelIndex)), ui->viennitView, SLOT(edit(QModelIndex)));
    connect( model->vientiModel(), SIGNAL(muuttunut()), this, SLOT(naytaSummat()));

    ui->viennitView->setItemDelegateForColumn( VientiModel::PVM, new PvmDelegaatti(ui->tositePvmEdit));
    ui->viennitView->setItemDelegateForColumn( VientiModel::TILI, new TiliDelegaatti() );
    ui->viennitView->setItemDelegateForColumn( VientiModel::DEBET, new EuroDelegaatti);
    ui->viennitView->setItemDelegateForColumn( VientiModel::KREDIT, new EuroDelegaatti);

    ui->viennitView->hideColumn(VientiModel::PROJEKTI);
    ui->viennitView->hideColumn(VientiModel::KUSTANNUSPAIKKA);
    ui->viennitView->horizontalHeader()->setStretchLastSection(true);

    ui->tunnisteEdit->setValidator( new QIntValidator(1,99999999) );

    connect( ui->lisaaRiviNappi, SIGNAL(clicked(bool)), this, SLOT(lisaaRivi()));
    connect( ui->tallennaButton, SIGNAL(clicked(bool)), this, SLOT(tallenna()));
    connect( ui->hylkaaNappi, SIGNAL(clicked(bool)), this, SLOT(hylkaa()));
    connect( ui->kommentitEdit, SIGNAL(textChanged()), this, SLOT(paivitaKommenttiMerkki()));

    connect( ui->tunnisteEdit, SIGNAL(textChanged(QString)), this, SLOT(tarkistaTunniste()));
    connect( ui->tositePvmEdit, SIGNAL(dateChanged(QDate)), this, SLOT(korjaaTunniste()));
    connect( ui->tositetyyppiCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(vaihdaTositeTyyppi()));

    // Tällä lukitaan tositteen pvm pysymään samalla tilikaudella jos tositteella on jo vientejä
    // connect( viennitModel, SIGNAL(vientejaOnTaiEi(bool)), this, SLOT(lukitseTilikaudelle(bool)));
    // Jos tosite kirjataan toiselle tilikaudelle, haetaan uusi numero
    connect( ui->tositePvmEdit, SIGNAL(dateChanged(QDate)), this, SLOT(tarkistaTunnisteJosTilikausiVaihtui(QDate)));

    connect( Kirjanpito::db(), SIGNAL(tietokantaVaihtui()), this, SLOT(hylkaa()) );
    ui->tositetyyppiCombo->setModel( Kirjanpito::db()->tositelajit());
    ui->tositetyyppiCombo->setModelColumn( TositelajiModel::NIMI);



    connect( ui->tositePvmEdit, SIGNAL(dateChanged(QDate)), model, SLOT(asetaPvm(QDate)));
    connect( ui->otsikkoEdit, SIGNAL(textChanged(QString)), model, SLOT(asetaOtsikko(QString)));

    // Tiliotteen tilivalintaan hyväksytään vain rahoitustilit

    QSortFilterProxyModel *proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel( kp()->tilit());
    proxy->setFilterRole( TiliModel::TyyppiRooli);
    proxy->setFilterFixedString("AR");
    proxy->setSortRole(TiliModel::NroRooli);

    ui->tiliotetiliCombo->setModel( proxy );
    ui->tiliotetiliCombo->setModelColumn(TiliModel::NRONIMI);


    ui->liiteView->setModel( model->liiteModel() );
}

KirjausWg::~KirjausWg()
{
    delete ui;
    delete model;
}

QDate KirjausWg::tositePvm() const
{
    return ui->tositePvmEdit->date();
}

void KirjausWg::lisaaRivi()
{
    model->vientiModel()->lisaaRivi();
    ui->viennitView->setFocus();

    QModelIndex indeksi = model->vientiModel()->index( model->vientiModel()->rowCount(QModelIndex()) - 1, VientiModel::TILI );

    ui->viennitView->setCurrentIndex( indeksi  );
    ui->viennitView->edit( indeksi );
}

void KirjausWg::tyhjenna()
{
    tositeId = 0;
    ui->tositePvmEdit->setDate( Kirjanpito::db()->paivamaara() );
    model->asetaPvm( kp()->paivamaara() );

    ui->otsikkoEdit->clear();
    ui->kommentitEdit->clear();

    ui->tunnisteEdit->clear();

    // Haetaan seuraava vapaa tunniste
    QSqlQuery query;

    ui->tunnisteEdit->setText( QString::number( seuraavaNumero() ) );

    ui->idLabel->clear();

    model->tyhjaa();
    ui->tabWidget->setCurrentIndex(0);
    // Laittaa samalla päivämäärärajat
    ui->tositePvmEdit->setDateRange(Kirjanpito::db()->tilitpaatetty().addDays(1), kp()->tilikaudet()->kirjanpitoLoppuu()  );

    ui->tositePvmEdit->setFocus();

    salliMuokkaus(true);
}

void KirjausWg::tallenna()
{
    model->asetaKommentti( ui->kommentitEdit->toPlainText() );
    model->asetaTunniste( ui->tunnisteEdit->text().toInt());

    if( ui->tilioteBox->isChecked())
    {
        model->asetaTiliotetili( ui->tiliotetiliCombo->currentData(TiliModel::IdRooli).toInt() );
        model->json()->set("TilioteAlkaa", ui->tiliotealkaenEdit->date());
        model->json()->set("TilioteLoppuu", ui->tilioteloppuenEdit->date());
    }
    else
    {
        model->asetaTiliotetili(0);
        model->json()->unset("TilioteAlkaa");
        model->json()->unset("TilioteLoppuu");
    }


    model->tallenna();

    /*
    // Ensin tarkistetaan, että täsmää
    int debetit = viennitModel->debetSumma();
    int kreditit = viennitModel->kreditSumma();

    if( debetit == 0 && kreditit == 0)
    {
        QMessageBox::critical(this, tr("Kitupiikki"), tr("Vientejä ei ole kirjattu"));
        return;
    }
    else if( debetit != kreditit)
    {
        // Kirjausten puolet eivät täsmää.
        if( QMessageBox::critical(this,tr("Kitupiikki"), tr("Debet- ja kredit-kirjaukset eivät täsmää.\n"
                                                       "Haluatko kuitenkin tallentaa kirjaukset?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes )
            return;
    }
    // Tarkistetaan, että joka rivillä on tili
    for(int i=0; i<viennitModel->rowCount(QModelIndex()); i++)
    {
        if( viennitModel->index(i, VientiModel::TILI).data().isNull() )
        {
            QMessageBox::critical(this, tr("Kitupiikki"), tr("Rivillä %1 tili on tyhjä").arg(i + 1));
            return;
        }
    }

    // Tallennuskysely
    QSqlQuery query;

    if( tositeId )
    {
        query.prepare("UPDATE tosite SET pvm=:pvm, otsikko=:otsikko, kommentti=:kommentti, tunniste=:tunniste, laji=:tyyppi "
                      "WHERE id=:id");
        query.bindValue(":id", tositeId);
    }
    else
        query.prepare("INSERT INTO tosite(pvm,otsikko,kommentti,tunniste,laji) values(:pvm,:otsikko,:kommentti,:tunniste,:tyyppi)");

    query.bindValue(":pvm", ui->tositePvmEdit->date());
    query.bindValue(":otsikko", ui->otsikkoEdit->text());
    query.bindValue(":kommentti", ui->kommentitEdit->document()->toPlainText());
    if( !ui->tunnisteEdit->text().isEmpty())
        query.bindValue(":tunniste", ui->tunnisteEdit->text());
    query.bindValue(":tyyppi",  ui->tositetyyppiCombo->currentData(TositelajiModel::IdRooli).toString());

    query.exec();



    if( !tositeId)
        tositeId = query.lastInsertId().toInt();

    if( !tositeId)
    {
        QMessageBox::critical(this,tr("Tallennus ei onnistu"),tr("Tositteen lisääminen ei onnistunut!"));
        return;
    }


    tositewg->tallennaTosite(tositeId); // Tallentaa tositetiedoston

    viennitModel->tallenna(tositeId);   // Tallentaa viennit

    Kirjanpito::db()->muokattu(); // Ilmoittaa, että kirjanpitoa on muokattu ja näkymät pitää päivittää

    tyhjenna(); // Tyhjennetään
    emit Kirjanpito::db()->palaaEdelliselleSivulle();
    */
}

void KirjausWg::hylkaa()
{
    tyhjenna();
    ui->tositetyyppiCombo->setCurrentIndex(0);
    emit Kirjanpito::db()->palaaEdelliselleSivulle();
}

void KirjausWg::naytaSummat()
{
    // ui->summaLabel->setText( tr("Debet %L1 €    Kredit %L2 €").arg(((double)viennitModel->debetSumma())/100.0 ,0,'f',2)
    //                         .arg(((double)viennitModel->kreditSumma()) / 100.0 ,0,'f',2));
}

void KirjausWg::lataaTosite(int id)
{
    model->lataa(id);

    ui->tositePvmEdit->setDate( model->pvm() );
    ui->otsikkoEdit->setText( model->otsikko() );
    ui->kommentitEdit->setPlainText( model->kommentti());
    ui->tunnisteEdit->setText( QString::number(model->tunniste()));
    ui->tositetyyppiCombo->setCurrentIndex( ui->tositetyyppiCombo->findData( model->tositelaji().id(), TositelajiModel::IdRooli ) );

    ui->tilioteBox->setChecked( model->tiliotetili() != 0 );
    // Tiliotetilin yhdistämiset!
    if( model->tiliotetili())
    {
        ui->tiliotetiliCombo->setCurrentIndex( ui->tiliotetiliCombo->findData( QVariant(model->tiliotetili()) ,TiliModel::IdRooli ) );
        ui->tiliotealkaenEdit->setDate( model->json()->date("TilioteAlkaa") );
        ui->tilioteloppuenEdit->setDate( model->json()->date("TilioteLoppuu"));
    }


    ui->tabWidget->setCurrentIndex(0);
    ui->tositePvmEdit->setFocus();

    return;

    /*

    QSqlQuery query;
    query.exec( QString("SELECT pvm, otsikko, kommentti, tunniste, tiedosto, tunniste, laji FROM tosite WHERE id=%1").arg(id) );
    if( query.next())
    {
        tositeId = id;

        // Jos systeemitosite taikka lukitulla ajalla, niin sitten ei voi muokata !
        if( query.value("laji")=="*" ||  ui->tositePvmEdit->date() <= Kirjanpito::db()->tilitpaatetty())
        {
            salliMuokkaus(false);
        }
        else
        {
            salliMuokkaus(true);
        }


        ui->tositePvmEdit->setDate( query.value("pvm").toDate() );
        ui->otsikkoEdit->setText( query.value("otsikko").toString());
        ui->kommentitEdit->setPlainText( query.value("kommentti").toString());
        ui->tunnisteEdit->setText( query.value("laji").toString());

        ui->tositetyyppiCombo->setCurrentIndex( ui->tositetyyppiCombo->findData( query.value("laji"), TositelajiModel::IdRooli ) );

        tositewg->tyhjenna( query.value("tunniste").toString(), query.value("tiedosto").toString() );

        ui->idLabel->setText(tr("# %1").arg(id));

        // Sitten ladataan vielä viennit
        viennitModel->lataa(id);
        naytaSummat();
        ui->tabWidget->setCurrentIndex(VIENNIT);


    }
    ui->tabWidget->setCurrentIndex(0);
    ui->tositePvmEdit->setFocus();
    */
}

void KirjausWg::paivitaKommenttiMerkki()
{
    if( ui->kommentitEdit->document()->toPlainText().isEmpty())
    {
        ui->tabWidget->setTabIcon(1, QIcon());
    }
    else
    {
        ui->tabWidget->setTabIcon(1, QIcon(":/pic/kommentti.png"));
    }

}

void KirjausWg::tarkistaTunniste()
{

    if( kelpaakoTunniste() )
    {
        ui->tunnisteEdit->setStyleSheet("color: black;");
        model->asetaTunniste( ui->tunnisteEdit->text().toInt() );
    }
    else
    {
        ui->tunnisteEdit->setStyleSheet("color: red;");
    }
}

void KirjausWg::korjaaTunniste()
{
    if( !kelpaakoTunniste())
        ui->tunnisteEdit->setText( QString::number(seuraavaNumero()) );
    tarkistaTunniste();
}

void KirjausWg::salliMuokkaus(bool sallitaanko)
{
    ui->tositePvmEdit->setEnabled(sallitaanko);
    ui->tositetyyppiCombo->setEnabled(sallitaanko);
    ui->kommentitEdit->setEnabled(sallitaanko);
    ui->tunnisteEdit->setEnabled(sallitaanko);
    ui->tallennaButton->setEnabled(sallitaanko);
    ui->otsikkoEdit->setEnabled(sallitaanko);

    // viennitModel->salliMuokkaus(sallitaanko);

    if(sallitaanko)
        ui->tositePvmEdit->setDateRange(Kirjanpito::db()->tilitpaatetty().addDays(1), kp()->tilikaudet()->kirjanpitoLoppuu() );
    else
        ui->tositePvmEdit->setDateRange( Kirjanpito::db()->tilikaudet()->kirjanpitoAlkaa() , Kirjanpito::db()->tilikaudet()->kirjanpitoLoppuu() );

}

void KirjausWg::vaihdaTositeTyyppi()
{
    ui->tyyppiLabel->setText( ui->tositetyyppiCombo->currentData(TositelajiModel::TunnusRooli).toString() );
    model->asetaTositelaji( ui->tositetyyppiCombo->currentData(TositelajiModel::IdRooli).toInt() );

    // ui->tyyppiLabel->setText( ui->tositetyyppiCombo->currentData().toString() );
    // Vaihdetaan myös numerointi uuden tunnistetyypin mukaiseksi
    ui->tunnisteEdit->setText( QString::number( seuraavaNumero() ) );
}

void KirjausWg::lukitseTilikaudelle(bool lukitaanko)
{
    if( lukitaanko )
    {
        Tilikausi tilikausi = Kirjanpito::db()->tilikausiPaivalle( ui->tositePvmEdit->date());
        ui->tositePvmEdit->setDateRange( tilikausi.alkaa(), tilikausi.paattyy() );
    }
    else
    {
        ui->tositePvmEdit->setDateRange(Kirjanpito::db()->tilitpaatetty().addDays(1), kp()->tilikaudet()->kirjanpitoLoppuu() );
    }
}

void KirjausWg::tarkistaTunnisteJosTilikausiVaihtui(QDate uusipaiva)
{
    if( edellinenpvm.isValid())
    {
        Tilikausi edellinen = Kirjanpito::db()->tilikausiPaivalle(edellinenpvm);
        Tilikausi nykyinen = Kirjanpito::db()->tilikausiPaivalle(uusipaiva);

        if( edellinen.alkaa() != nykyinen.alkaa() )
            // Vaihdetaan tunnistenumero
            ui->tunnisteEdit->setText( QString::number( seuraavaNumero() ) );
    }
    edellinenpvm = uusipaiva;
}


int KirjausWg::seuraavaNumero()
{
    Tilikausi kausi = Kirjanpito::db()->tilikausiPaivalle( ui->tositePvmEdit->date());

    QString kysymys = QString("SELECT max(tunniste) FROM tosite WHERE "
                    " pvm BETWEEN \"%1\" AND \"%2\" "
                    " AND laji=\"%3\" ")
                                .arg(kausi.alkaa().toString(Qt::ISODate))
                                .arg(kausi.paattyy().toString(Qt::ISODate))
                                .arg( ui->tositetyyppiCombo->currentData(TositelajiModel::IdRooli).toInt());
    QSqlQuery kysely;
    kysely.exec(kysymys);
    if( kysely.next())
        return kysely.value(0).toInt() + 1;
    else
        return 1;
}

bool KirjausWg::kelpaakoTunniste()
{
    // Onko kyseisellä kaudella ja samalla tyypillä
    Tilikausi kausi = Kirjanpito::db()->tilikausiPaivalle( ui->tositePvmEdit->date() );
    QString kysymys = QString("SELECT id FROM tosite WHERE tunniste=\"%1\" "
                              "AND pvm BETWEEN \"%2\" AND \"%3\" AND id <> %4 "
                              "AND laji=\"%5\" ").arg(ui->tunnisteEdit->text())
                                                          .arg(kausi.alkaa().toString(Qt::ISODate))
                                                          .arg(kausi.paattyy().toString(Qt::ISODate))
                                                          .arg(tositeId)
                                                          .arg(ui->tositetyyppiCombo->currentData(TositelajiModel::IdRooli).toInt() );
    QSqlQuery kysely;
    kysely.exec(kysymys);
    return !kysely.next();
}
