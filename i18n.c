/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vdr/config.h>
#include "i18n.h"

const char *ISO639code[] = {
  // Language ISO 639 codes for DVD
    "en",
    "de",
    "sl",
    "it",
    "nl",
    "pt",
    "fr",
    "no",
    "fi",
    "pl",
    "es",
    "el",
    "se",
    "ro",
    "hu",
    "ca",
    "ru",
    "hr",
    "et",
    "da",
    "cs",
#if VDRVERSNUM >= 10502
    "tr",
#endif
};

const char *DvdLanguageCode[][2] = {
  // Language ISO 639 codes for DVD
    {"en", "English"},
    {"de", "Deutsch"},
    {"sl", "Slovenski"},
    {"it", "Italiano"},
    {"nl", "Nederlands"},
    {"pt", "Portugu�s"},
    {"fr", "Fran�ais"},
    {"no", "Norsk"},
    {"fi", "suomi"},
    {"pl", "Polski"},
    {"es", "Espa�ol"},
    {"el", "��������"},
    {"se", "Svenska"},
    {"ro", "Romaneste"},
    {"hu", "Magyar"},
    {"ca", "Catal�"},
    {"ru", "Русский"},
    {"hr", "Hrvatski"},
    {"et", "Eesti"},
    {"da", "Dansk"},
    {"cs", "Czech"},
    {"tr", "T�rk�e"}
};

#if VDRVERSNUM < 10507
const tI18nPhrase DvdPhrases[] = {
    {
    "Plugin.DVD$DVD",                                       // English
        "DVD",                                              // Deutsch
        "DVD",                                              // Slovenski
        "DVD",                                              // Italiano
        "DVD",                                              // Nederlands
        "DVD",                                              // Portugu�s
        "DVD",                                              // Fran�ais
        "DVD",                                              // Norsk
        "DVD",                                              // suomi
        "DVD",                                              // Polski
        "DVD",                                              // Espa�ol
        "DVD",                                              // �������� (Greek)
        "DVD",                                              // Svenska
        "DVD",                                              // Romaneste
        "DVD",                                              // Magyar
        "DVD",                                              // Catal�
        "������������� DVD",                                // ������� (Russian)
        "DVD",                                              // Hrvatski (Croatian)
        "DVD",                                              // Eesti
        "DVD",                                              // Dansk
        "DVD",                                              // Czech
#if VDRVERSNUM >= 10502
        "DVD",                                              // T�rk�e
#endif
    },
    {
    "Plugin.DVD$turn VDR into an (almost) full featured DVD player",    // English
        "verwandelt VDR in einen (fast) vollst�ndigen DVD Spieler",     // Deutsch
        "spremeni VDR v (skoraj) popolen DVD predvajalnik",             // Slovenski
        "",                                                             // Italiano
        "Verander VDR in een (bijna) complete DVD-speler",              // Nederlands
        "",                                                             // Portugu�s
        "",                                                             // Fran�ais
        "",                                                             // Norsk
        "DVD-soitin",                                                   // suomi
        "Zmienia VDR w odtwarzacz DVD",                                 // Polski
        "Convierte VDR en un reproductor DVD (casi) completo",          // Espa�ol
        "Metatropi tou VDR se ena (sxedon) olokliromeno DVD",           // �������� (Greek)
        "",                                                             // Svenska
        "",                                                             // Romaneste
        "",                                                             // Magyar
        "",                                                             // Catal�
        "������������� DVD",                                            // ������� (Russian)
        "",                                                             // Hrvatski (Croatian)
        "DVD-m�ngija",                                                  // Eesti
        "forvandler VDR til en (n�sten) normal DVD afspiller",          // Dansk
        "P�em�n� VDR v plnohodnotn� DVD p�ehr�va�",                     // Czech
#if VDRVERSNUM >= 10502
        "VDR'i (nerdeyse) tam DVD oynat�c�s�na �evirir"                 // T�rk�e
#endif
    },
    {
    "Setup.DVD$Preferred menu language",                    // English
        "Bevorzugte Sprache f�r Men�s",                     // Deutsch
        "Prednostni jezik za menije",                       // Slovenski
        "menu - linguaggio preferito",                      // Italiano
        "Taalkeuze voor menu",                              // Nederlands
        "",                                                 // Portugu�s
        "Langage pr�f�r� pour les menus",                   // Fran�ais
        "",                                                 // Norsk
        "Haluttu valikkokieli",                             // suomi
        "Preferowany j�zyk menu",                           // Polski
        "Idioma preferido para los men�s",                  // Espa�ol
        "Protinomeni glossa gia menou",                     // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "�������������� ���� ����",                         // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud men��keel",                             // Eesti
        "Foretrukket sprog for menuer",                     // Dansk
        "Preferovan� jazyk menu",                           // Czech
#if VDRVERSNUM >= 10502
        "Men�lerin dil tercihi"                             // T�rk�e
#endif
    },
    {
    "Setup.DVD$Preferred audio language",                   // English
        "Bevorzugte Sprache f�r Dialog",                    // Deutsch
        "Prednostni jezik za zvok",                         // Slovenski
        "audio - linguaggio preferito",                     // Italiano
        "Taalkeuze voor geluid",                            // Nederlands
        "",                                                 // Portugu�s
        "Langage pr�f�r� pour le son",                      // Fran�ais
        "",                                                 // Norsk
        "Haluttu ��niraita",                                // suomi
        "Preferowany j�zyk d�wi�ku",                        // Polski
        "Idioma preferido para el sonido",                  // Espa�ol
        "Protinomeni glossa gia ton dialogo",               // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "�������������� ���� �����",                        // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud audiokeel",                             // Eesti
        "Foretrukket sprog for lyd",                        // Dansk
        "Preferovan� jazyk zvuku",                          // Czech
#if VDRVERSNUM >= 10502
        "Diyaloglar�n dil tercihi"                          // T�rk�e
#endif
    },
    {
    "Setup.DVD$Preferred subtitle language",                // English
        "Bevorzugte Sprache f�r Untertitel",                // Deutsch
        "Prednostni jezik za podnapise",                    // Slovenski
        "sottotitoli - linguaggio preferito",               // Italiano
        "Taalkeuze voor ondertitels",                       // Nederlands
        "",                                                 // Portugu�s
        "Langage pr�f�r� pour les sous-titres",             // Fran�ais
        "",                                                 // Norsk
        "Haluttu tekstityskieli",                           // suomi
        "Preferowany j�zyk napis�w",                        // Polski
        "Idioma preferido para los subt�tulos",             // Espa�ol
        "Protinomeni glossa ipotitlon",                     // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "�������������� ���� ���������",                    // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud subtiitritekeel",                       // Eesti
        "Foretrukket sprog for undertekster",               // Dansk
        "Preferovan� jazyk titulk�",                        // Czech
#if VDRVERSNUM >= 10502
        "Altyaz�lar�n dil tercihi"                          // T�rk�e
#endif
    },
    {
    "Setup.DVD$Player region code",                         // English
        "Regionalkode f�r DVD Spieler",                     // Deutsch
        "Regionalna koda za predvajalnik",                  // Slovenski
        "region code del DVD player",                       // Italiano
        "Regiocode van Speler",                             // Nederlands
        "",                                                 // Portugu�s
        "Code r�gion du lecteur",                           // Fran�ais
        "",                                                 // Norsk
        "Soittimen aluekoodi",                              // suomi
        "Kod regionu odtwarzacza",                          // Polski
        "C�digo de zona del lector",                        // Espa�ol
        "Kodikos Zonis DVD",                                // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "��� ����",                                         // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "M�ngija regioonikood",                             // Eesti
        "Afspillerens regions kode",                        // Dansk
        "Region�ln� k�d p�ehr�va�e",                        // Czech
#if VDRVERSNUM >= 10502
        "DVD oynat�c�n�n b�lge kodu"                        // T�rk�e
#endif
    },
    {
    "Setup.DVD$Display subtitles",                          // English
        "Untertitel anzeigen",                              // Deutsch
        "Prika�i podnapise",                                // Slovenski
        "Visualizza sottotitoli",                           // Italiano
        "Toon ondertitels",                                 // Nederlands
        "",                                                 // Portugu�s
        "Affiche les sous-titres",                          // Fran�ais
        "",                                                 // Norsk
        "N�yt� tekstitys",                                  // suomi
        "Wy�wietlaj napisy",                                // Polski
        "Mostrar subt�tulos",                               // Espa�ol
        "Endiksi ipotitlon",                                // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "���������� ��������",                              // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitrite kuvamine",                             // Eesti
        "Vis undertekster",                                 // Dansk
        "Zobrazovat titulky",                               // Czech
#if VDRVERSNUM >= 10502
        "Altyaz�lar� g�ster"                                // T�rk�e
#endif
    },
    {
    "Setup.DVD$Hide Mainmenu Entry",
        "Hauptmen�eintrag verstecken",                      // Deutsch
        "Skrij galvni meni",                                // Slovenski
        "Nascondi voce men�",                               // Italiano
        "Verberg vermelding in het hoofdmenu",              // Nederlands
        "",                                                 // Portugu�s
        "",                                                 // Fran�ais
        "",                                                 // Norsk
        "Piilota valinta p��valikosta",                     // suomi
        "Ukryj pozycj� w g��wnym menu",                     // Polski
        "Ocultar entrada en el men� principal",             // Espa�ol
        "Apokripsi sto vasiko menou",                       // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "������ ������� � ������� ����",                    // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Peita peamen�� valikust",                          // Eesti
        "Skjul DVD i hovedmenuen",                          // Dansk
        "Skr�t polo�ku v hlavn�m menu",                     // Czech
#if VDRVERSNUM >= 10502
        "Ana men�de sakla"                                  // T�rk�e
#endif
    },
    {
    "Setup.DVD$ReadAHead",
        "ReadAHead",                                        // Deutsch
        "Beri vnaprej",                                       // Slovenski
        "ReadAHead",                                        // Italiano
        "Vooruit lezen",                                    // Nederlands
        "ReadAHead",                                        // Portugu�s
        "ReadAHead",                                        // Fran�ais
        "ReadAHead",                                        // Norsk
        "ReadAHead-toiminto",                               // suomi
        "ReadAHead",                                        // Polski
        "Lectura anticipada",                               // Espa�ol
        "ReadAHead",                                        // �������� (Greek)
        "ReadAHead",                                        // Svenska
        "ReadAHead",                                        // Romaneste
        "ReadAHead",                                        // Magyar
        "ReadAHead",                                        // Catal�
        "����������� ������",                               // ������� (Russian)
        "ReadAHead",                                        // Hrvatski (Croatian)
        "ReadAHead",                                        // Eesti
        "L�s forud",                                        // Dansk
        "Dop�edn� �ten�",                                   // Czech
#if VDRVERSNUM >= 10502
        "�leri oku"                                         // T�rk�e
#endif
    },
    {
    "Setup.DVD$Gain (analog)",
        "Verst�rkung (analog)",                             // Deutsch
        "Oja�anje (analogno)",                              // Slovenski
        "Gain (analog)",                                    // Italiano
        "Versterking (analoog)",                            // Nederlands
        "Gain (analog)",                                    // Portugu�s
        "Gain (analog)",                                    // Fran�ais
        "Gain (analog)",                                    // Norsk
        "��nen vahvistus (analoginen)",                     // suomi
        "Zysk (analogowo)",                                 // Polski
        "Ganancia (anal�gico)",                             // Espa�ol
        "Gain (analogika)",                                 // �������� (Greek)
        "Gain (analog)",                                    // Svenska
        "Gain (analog)",                                    // Romaneste
        "Gain (analog)",                                    // Magyar
        "Gain (analog)",                                    // Catal�
        "�������� (������.)",                               // ������� (Russian)
        "Gain (analog)",                                    // Hrvatski (Croatian)
        "Heliv�imendamine (analoog)",                       // Eesti
        "Forst�rkning (analog)",                            // Dansk
        "Zes�len� (analog)",                                // Czech
#if VDRVERSNUM >= 10502
        "Kuvvetlendirmek (analog)"                          // T�rk�e
#endif
    },
    {
    "Error.DVD$Error opening DVD!",                         // English
        "Fehler beim �ffnen der DVD!",                      // Deutsch
        "Napaka pri odpiranju DVD-ja!",                     // Slovenski
        "",                                                 // Italiano
        "Fout bij het openen van de DVD!",                  // Nederlands
        "",                                                 // Portugu�s
        "",                                                 // Fran�ais
        "",                                                 // Norsk
        "DVD:n avaaminen ep�onnistui!",                     // suomi
        "B��d otwierania DVD!",                             // Polski
        "�Error abriendo el DVD!",                          // Espa�ol
        "Lathos sto anigma tou DVD",                        // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "������ �������� DVD!",                             // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "DVD avamine eba�nnestus!",                         // Eesti
        "Fejl ved �bning af DVD!",                          // Dansk
        "Chyba p�i otev�r�n� DVD!",                         // Czech
#if VDRVERSNUM >= 10502
        "DVD a��l�� hatas�!"                                // T�rk�e
#endif
    },
    {
    "Error.DVD$Error fetching data from DVD!",              // English
        "Fehler beim Lesen von der DVD!",                   // Deutsch
        "Napaka pri branju podatkov iz DVD-ja!",            // Slovenski
        "",                                                 // Italiano
        "Error bij het verkrijgen van data van de DVD!",    // Nederlands
        "",                                                 // Portugu�s
        "",                                                 // Fran�ais
        "",                                                 // Norsk
        "DVD:n lukeminen ep�onnistui",                      // suomi
        "B��d pobierania danych z DVD!",                    // Polski
        "�Error obteniendo datos del DVD!",                 // Espa�ol
        "Lathos sto diavasma tou DVD",                      // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "������ ������ DVD!",                               // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Lugemine DVD-lt eba�nnestus!",                     // Eesti
        "Fejl ved hentning af data fra DVD!",               // Dansk
        "Chyba p�i na��t�n� dat z DVD!",                    // Czech
#if VDRVERSNUM >= 10502
        "DVD okuma hatas�!"                                 // T�rk�e
#endif
    },
    {
    "Error.DVD$Current subtitle stream not seen!",          // English
        "Der ausgew�hlte Untertitel ist nicht vorhanden!",  // Deutsch
        "Izbrani podnapisi niso prisotni!",                 // Slovenski
        "",                                                 // Italiano
        "De gekozen ondertiteling is niet beschikbaar!",    // Nederlands
        "",                                                 // Portugu�s
        "",                                                 // Fran�ais
        "",                                                 // Norsk
        "Tekstitysraitaa ei havaita!",                      // suomi
        "Wybrane napisy nie istniej�!",                     // Polski
        "�Subt�tulos actuales no encontrados!",             // Espa�ol
        "O epilegmenos ipotitlos den iparxi",               // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "���������� ���������� �������� �������!",          // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitriterada pole avastatud!",                  // Eesti
        "Valgte undertekst findes ikke!",                   // Dansk
        "Vybran� titulky nejsou k dispozici!",              // Czech
#if VDRVERSNUM >= 10502
        "Se�ilen altyaz� bulunamad�!"                       // T�rk�e
#endif
    },
    {
    "Error.DVD$Current audio track not seen!",              // English
        "Die ausgew�hlte Audiospur ist nicht vorhanden!",   // Deutsch
        "Izbrani zvo�ni zapis ni prisoten!",                // Slovenski
        "",                                                 // Italiano
        "Het gekozen audiospoort is niet beschikbaar",      // Nederlands
        "",                                                 // Portugu�s
        "",                                                 // Fran�ais
        "",                                                 // Norsk
        "��niraitaa ei havaita!",                           // suomi
        "Wybrana �cie�ka d�wi�kowa nie istnieje!",          // Polski
        "�Pista de audio actual no encontrada!",            // Espa�ol
        "I Epilegmeni desmi ixou den mpori na wrethi",      // �������� (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal�
        "���������� ������� ����������!",                   // ������� (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Audiorada pole avastatud!",                        // Eesti
        "Valgte lyd-spor findes ikke!",                     // Dansk
        "Vybran� zvukov� stopa nen� k dispozici!",          // Czech
#if VDRVERSNUM >= 10502
        "Se�ilen audio ses bulunamad�!"                     // T�rk�e
#endif
    },
    { NULL }
};
#endif
