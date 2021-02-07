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
    {"pt", "Português"},
    {"fr", "Français"},
    {"no", "Norsk"},
    {"fi", "suomi"},
    {"pl", "Polski"},
    {"es", "Español"},
    {"el", "ÅëëçíéêÜ"},
    {"se", "Svenska"},
    {"ro", "Romaneste"},
    {"hu", "Magyar"},
    {"ca", "Català"},
    {"ru", "Ğ ÑƒÑÑĞºĞ¸Ğ¹"},
    {"hr", "Hrvatski"},
    {"et", "Eesti"},
    {"da", "Dansk"},
    {"cs", "Czech"},
    {"tr", "Türkçe"}
};

#if VDRVERSNUM < 10507
const tI18nPhrase DvdPhrases[] = {
    {
    "Plugin.DVD$DVD",                                       // English
        "DVD",                                              // Deutsch
        "DVD",                                              // Slovenski
        "DVD",                                              // Italiano
        "DVD",                                              // Nederlands
        "DVD",                                              // Português
        "DVD",                                              // Français
        "DVD",                                              // Norsk
        "DVD",                                              // suomi
        "DVD",                                              // Polski
        "DVD",                                              // Español
        "DVD",                                              // ÅëëçíéêÜ (Greek)
        "DVD",                                              // Svenska
        "DVD",                                              // Romaneste
        "DVD",                                              // Magyar
        "DVD",                                              // Català
        "¿àŞØÓàëÒĞâÕÛì DVD",                                // ÀãááÚØÙ (Russian)
        "DVD",                                              // Hrvatski (Croatian)
        "DVD",                                              // Eesti
        "DVD",                                              // Dansk
        "DVD",                                              // Czech
#if VDRVERSNUM >= 10502
        "DVD",                                              // Türkçe
#endif
    },
    {
    "Plugin.DVD$turn VDR into an (almost) full featured DVD player",    // English
        "verwandelt VDR in einen (fast) vollständigen DVD Spieler",     // Deutsch
        "spremeni VDR v (skoraj) popolen DVD predvajalnik",             // Slovenski
        "",                                                             // Italiano
        "Verander VDR in een (bijna) complete DVD-speler",              // Nederlands
        "",                                                             // Português
        "",                                                             // Français
        "",                                                             // Norsk
        "DVD-soitin",                                                   // suomi
        "Zmienia VDR w odtwarzacz DVD",                                 // Polski
        "Convierte VDR en un reproductor DVD (casi) completo",          // Español
        "Metatropi tou VDR se ena (sxedon) olokliromeno DVD",           // ÅëëçíéêÜ (Greek)
        "",                                                             // Svenska
        "",                                                             // Romaneste
        "",                                                             // Magyar
        "",                                                             // Català
        "¿àŞØÓàëÒĞâÕÛì DVD",                                            // ÀãááÚØÙ (Russian)
        "",                                                             // Hrvatski (Croatian)
        "DVD-mängija",                                                  // Eesti
        "forvandler VDR til en (næsten) normal DVD afspiller",          // Dansk
        "Pøemìní VDR v plnohodnotnı DVD pøehrávaè",                     // Czech
#if VDRVERSNUM >= 10502
        "VDR'i (nerdeyse) tam DVD oynatıcısına çevirir"                 // Türkçe
#endif
    },
    {
    "Setup.DVD$Preferred menu language",                    // English
        "Bevorzugte Spache für Menüs",                      // Deutsch
        "Prednostni jezik za menije",                       // Slovenski
        "menu - linguaggio preferito",                      // Italiano
        "Taalkeuze voor menu",                              // Nederlands
        "",                                                 // Português
        "Langage préféré pour les menus",                   // Français
        "",                                                 // Norsk
        "Haluttu valikkokieli",                             // suomi
        "Preferowany jêzyk menu",                           // Polski
        "Idioma preferido para los menús",                  // Español
        "Protinomeni glossa gia menou",                     // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿àÕÔßŞçØâĞÕÜëÙ ï×ëÚ ÜÕİî",                         // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud menüükeel",                             // Eesti
        "Foretrukket sprog for menuer",                     // Dansk
        "Preferovanı jazyk menu",                           // Czech
#if VDRVERSNUM >= 10502
        "Menülerin dil tercihi"                             // Türkçe
#endif
    },
    {
    "Setup.DVD$Preferred audio language",                   // English
        "Bevorzugte Sprache für Dialog",                    // Deutsch
        "Prednostni jezik za zvok",                         // Slovenski
        "audio - linguaggio preferito",                     // Italiano
        "Taalkeuze voor geluid",                            // Nederlands
        "",                                                 // Português
        "Langage préféré pour le son",                      // Français
        "",                                                 // Norsk
        "Haluttu ääniraita",                                // suomi
        "Preferowany jêzyk d¼wiêku",                        // Polski
        "Idioma preferido para el sonido",                  // Español
        "Protinomeni glossa gia ton dialogo",               // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿àÕÔßŞçØâĞÕÜëÙ ï×ëÚ ĞãÔØŞ",                        // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud audiokeel",                             // Eesti
        "Foretrukket sprog for lyd",                        // Dansk
        "Preferovanı jazyk zvuku",                          // Czech
#if VDRVERSNUM >= 10502
        "Diyalogların dil tercihi"                          // Türkçe
#endif
    },
    {
    "Setup.DVD$Preferred subtitle language",                // English
        "Bevorzugte Spache für Untertitel",                 // Deutsch
        "Prednostni jezik za podnapise",                    // Slovenski
        "sottotitoli - linguaggio preferito",               // Italiano
        "Taalkeuze voor ondertitels",                       // Nederlands
        "",                                                 // Português
        "Langage préféré pour les sous-titres",             // Français
        "",                                                 // Norsk
        "Haluttu tekstityskieli",                           // suomi
        "Preferowany jêzyk napisów",                        // Polski
        "Idioma preferido para los subtítulos",             // Español
        "Protinomeni glossa ipotitlon",                     // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿àÕÔßŞçØâĞÕÜëÙ ï×ëÚ áãÑâØâàŞÒ",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud subtiitritekeel",                       // Eesti
        "Foretrukket sprog for undertekster",               // Dansk
        "Preferovanı jazyk titulkù",                        // Czech
#if VDRVERSNUM >= 10502
        "Altyazıların dil tercihi"                          // Türkçe
#endif
    },
    {
    "Setup.DVD$Player region code",                         // English
        "Regionalkode für DVD Spieler",                     // Deutsch
        "Regionalna koda za predvajalnik",                  // Slovenski
        "region code del DVD player",                       // Italiano
        "Regiocode van Speler",                             // Nederlands
        "",                                                 // Português
        "Code région du lecteur",                           // Français
        "",                                                 // Norsk
        "Soittimen aluekoodi",                              // suomi
        "Kod regionu odtwarzacza",                          // Polski
        "Código de zona del lector",                        // Español
        "Kodikos Zonis DVD",                                // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "ºŞÔ ×Şİë",                                         // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Mängija regioonikood",                             // Eesti
        "Afspillerens regions kode",                        // Dansk
        "Regionální kód pøehrávaèe",                        // Czech
#if VDRVERSNUM >= 10502
        "DVD oynatıcının bölge kodu"                        // Türkçe
#endif
    },
    {
    "Setup.DVD$Display subtitles",                          // English
        "Untertitel anzeigen",                              // Deutsch
        "Prika¾i podnapise",                                // Slovenski
        "Visualizza sottotitoli",                           // Italiano
        "Toon ondertitels",                                 // Nederlands
        "",                                                 // Português
        "Affiche les sous-titres",                          // Français
        "",                                                 // Norsk
        "Näytä tekstitys",                                  // suomi
        "Wy¶wietlaj napisy",                                // Polski
        "Mostrar subtítulos",                               // Español
        "Endiksi ipotitlon",                                // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿ŞÚĞ×ëÒĞâì áãÑâØâàë",                              // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitrite kuvamine",                             // Eesti
        "Vis undertekster",                                 // Dansk
        "Zobrazovat titulky",                               // Czech
#if VDRVERSNUM >= 10502
        "Altyazıları göster"                                // Türkçe
#endif
    },
    {
    "Setup.DVD$Hide Mainmenu Entry",
        "Hauptmenüeintrag verstecken",                      // Deutsch
        "Skrij galvni meni",                                // Slovenski
        "Nascondi voce menù",                               // Italiano
        "Verberg vermelding in het hoofdmenu",              // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "Piilota valinta päävalikosta",                     // suomi
        "Ukryj pozycjê w g³ównym menu",                     // Polski
        "Ocultar entrada en el menú principal",             // Español
        "Apokripsi sto vasiko menou",                       // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "ÁÚàëâì ÚŞÜĞİÔã Ò ÓÛĞÒİŞÜ ÜÕİî",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Peita peamenüü valikust",                          // Eesti
        "Skjul DVD i hovedmenuen",                          // Dansk
        "Skrıt polo¾ku v hlavním menu",                     // Czech
#if VDRVERSNUM >= 10502
        "Ana menüde sakla"                                  // Türkçe
#endif
    },
    {
    "Setup.DVD$ReadAHead",
        "ReadAHead",                                        // Deutsch
        "Beri vnaprej",                                       // Slovenski
        "ReadAHead",                                        // Italiano
        "Vooruit lezen",                                    // Nederlands
        "ReadAHead",                                        // Português
        "ReadAHead",                                        // Français
        "ReadAHead",                                        // Norsk
        "ReadAHead-toiminto",                               // suomi
        "ReadAHead",                                        // Polski
        "Lectura anticipada",                               // Español
        "ReadAHead",                                        // ÅëëçíéêÜ (Greek)
        "ReadAHead",                                        // Svenska
        "ReadAHead",                                        // Romaneste
        "ReadAHead",                                        // Magyar
        "ReadAHead",                                        // Català
        "ÃßàÕÖÔĞîéÕÕ çâÕİØÕ",                               // ÀãááÚØÙ (Russian)
        "ReadAHead",                                        // Hrvatski (Croatian)
        "ReadAHead",                                        // Eesti
        "Læs forud",                                        // Dansk
        "Dopøedné ètení",                                   // Czech
#if VDRVERSNUM >= 10502
        "İleri oku"                                         // Türkçe
#endif
    },
    {
    "Setup.DVD$Gain (analog)",
        "Verstärkung (analog)",                             // Deutsch
        "Ojaèanje (analogno)",                              // Slovenski
        "Gain (analog)",                                    // Italiano
        "Versterking (analoog)",                            // Nederlands
        "Gain (analog)",                                    // Português
        "Gain (analog)",                                    // Français
        "Gain (analog)",                                    // Norsk
        "Äänen vahvistus (analoginen)",                     // suomi
        "Zysk (analogowo)",                                 // Polski
        "Ganancia (analógico)",                             // Español
        "Gain (analogika)",                                 // ÅëëçíéêÜ (Greek)
        "Gain (analog)",                                    // Svenska
        "Gain (analog)",                                    // Romaneste
        "Gain (analog)",                                    // Magyar
        "Gain (analog)",                                    // Català
        "ÃáØÛÕİØÕ (ĞİĞÛŞÓ.)",                               // ÀãááÚØÙ (Russian)
        "Gain (analog)",                                    // Hrvatski (Croatian)
        "Helivõimendamine (analoog)",                       // Eesti
        "Forstærkning (analog)",                            // Dansk
        "Zesílení (analog)",                                // Czech
#if VDRVERSNUM >= 10502
        "Kuvvetlendirmek (analog)"                          // Türkçe
#endif
    },
    {
    "Error.DVD$Error opening DVD!",                         // English
        "Fehler beim öffnen der DVD!",                      // Deutsch
        "Napaka pri odpiranju DVD-ja!",                     // Slovenski
        "",                                                 // Italiano
        "Fout bij het openen van de DVD!",                  // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "DVD:n avaaminen epäonnistui!",                     // suomi
        "B³±d otwierania DVD!",                             // Polski
        "¡Error abriendo el DVD!",                          // Español
        "Lathos sto anigma tou DVD",                        // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¾èØÑÚĞ ŞâÚàëâØï DVD!",                             // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "DVD avamine ebaõnnestus!",                         // Eesti
        "Fejl ved åbning af DVD!",                          // Dansk
        "Chyba pøi otevírání DVD!",                         // Czech
#if VDRVERSNUM >= 10502
        "DVD açılış hatası!"                                // Türkçe
#endif
    },
    {
    "Error.DVD$Error fetching data from DVD!",              // English
        "Fehler beim lesen von der DVD!",                   // Deutsch
        "Napaka pri branju podatkov iz DVD-ja!",            // Slovenski
        "",                                                 // Italiano
        "Error bij het verkrijgen van data van de DVD!",    // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "DVD:n lukeminen epäonnistui",                      // suomi
        "B³±d pobierania danych z DVD!",                    // Polski
        "¡Error obteniendo datos del DVD!",                 // Español
        "Lathos sto diavasma tou DVD",                      // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¾èØÑÚĞ çâÕİØï DVD!",                               // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Lugemine DVD-lt ebaõnnestus!",                     // Eesti
        "Fejl ved hentning af data fra DVD!",               // Dansk
        "Chyba pøi naèítání dat z DVD!",                    // Czech
#if VDRVERSNUM >= 10502
        "DVD okuma hatası!"                                 // Türkçe
#endif
    },
    {
    "Error.DVD$Current subtitle stream not seen!",          // English
        "Der ausgewählte Untertitel ist nicht vorhanden!",  // Deutsch
        "Izbrani podnapisi niso prisotni!",                 // Slovenski
        "",                                                 // Italiano
        "De gekozen ondertiteling is niet beschikbaar!",    // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "Tekstitysraitaa ei havaita!",                      // suomi
        "Wybrane napisy nie istniej±!",                     // Polski
        "¡Subtítulos actuales no encontrados!",             // Español
        "O epilegmenos ipotitlos den iparxi",               // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "½ÕÔŞáâãßÕİ ÒØÔÕŞßŞâŞÚ âÕÚãéÕÓŞ àĞ×ÔÕÛĞ!",          // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitriterada pole avastatud!",                  // Eesti
        "Valgte undertekst findes ikke!",                   // Dansk
        "Vybrané titulky nejsou k dispozici!",              // Czech
#if VDRVERSNUM >= 10502
        "Seçilen altyazı bulunamadı!"                       // Türkçe
#endif
    },
    {
    "Error.DVD$Current audio track not seen!",              // English
        "Die ausgewählte Audiospur ist nicht vorhanden!",   // Deutsch
        "Izbrani zvoèni zapis ni prisoten!",                // Slovenski
        "",                                                 // Italiano
        "Het gekozen audiospoort is niet beschikbaar",      // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "Ääniraitaa ei havaita!",                           // suomi
        "Wybrana ¶cie¿ka d¼wiêkowa nie istnieje!",          // Polski
        "¡Pista de audio actual no encontrada!",            // Español
        "I Epilegmeni desmi ixou den mpori na wrethi",      // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "½ÕÔŞáâãßÕİ âÕÚãéØÙ ĞãÔØŞßŞâŞÚ!",                   // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Audiorada pole avastatud!",                        // Eesti
        "Valgte lyd-spor findes ikke!",                     // Dansk
        "Vybraná zvuková stopa není k dispozici!",          // Czech
#if VDRVERSNUM >= 10502
        "Seçilen audio ses bulunamadı!"                     // Türkçe
#endif
    },
    { NULL }
};
#endif
