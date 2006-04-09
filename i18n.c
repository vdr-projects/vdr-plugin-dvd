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
#if VDRVERSNUM >= 10342
    "cs",
#endif
};

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
        "¿àÞØÓàëÒÐâÕÛì DVD",                                // ÀãááÚØÙ (Russian)
        "DVD",                                              // Hrvatski (Croatian)
        "DVD",                                              // Eesti
        "DVD",                                              // Dansk
#if VDRVERSNUM >= 10342
        "DVD",                                              // Czech
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
        "¿àÞØÓàëÒÐâÕÛì DVD",                                            // ÀãááÚØÙ (Russian)
        "",                                                             // Hrvatski (Croatian)
        "DVD-mängija",                                                  // Eesti
        "forvandler VDR til en (næsten) normal DVD afspiller",          // Dansk
#if VDRVERSNUM >= 10342
        "Pøemìní VDR v plnohodnotný DVD pøehrávaè",                     // Czech
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
        "¿àÕÔßÞçØâÐÕÜëÙ ï×ëÚ ÜÕÝî",                         // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud menüükeel",                             // Eesti
        "Foretrukket sprog for menuer",                     // Dansk
#if VDRVERSNUM >= 10342
        "Preferovaný jazyk menu",                           // Czech
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
        "¿àÕÔßÞçØâÐÕÜëÙ ï×ëÚ ÐãÔØÞ",                        // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud audiokeel",                             // Eesti
        "Foretrukket sprog for lyd",                        // Dansk
#if VDRVERSNUM >= 10342
        "Preferovaný jazyk zvuku",                          // Czech
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
        "¿àÕÔßÞçØâÐÕÜëÙ ï×ëÚ áãÑâØâàÞÒ",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud subtiitritekeel",                       // Eesti
        "Foretrukket sprog for undertekster",               // Dansk
#if VDRVERSNUM >= 10342
        "Preferovaný jazyk titulkù",                        // Czech
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
        "ºÞÔ ×ÞÝë",                                         // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Mängija regioonikood",                             // Eesti
        "Afspillerens regions kode",                        // Dansk
#if VDRVERSNUM >= 10342
        "Regionální kód pøehrávaèe",                        // Czech
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
        "¿ÞÚÐ×ëÒÐâì áãÑâØâàë",                              // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitrite kuvamine",                             // Eesti
        "Vis undertekster",                                 // Dansk
#if VDRVERSNUM >= 10342
        "Zobrazovat titulky",                               // Czech
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
        "ÁÚàëâì ÚÞÜÐÝÔã Ò ÓÛÐÒÝÞÜ ÜÕÝî",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Peita peamenüü valikust",                          // Eesti
        "Skjul DVD i hovedmenuen",                          // Dansk
#if VDRVERSNUM >= 10342
        "Skrýt polo¾ku v hlavním menu",                     // Czech
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
        "ÃßàÕÖÔÐîéÕÕ çâÕÝØÕ",                               // ÀãááÚØÙ (Russian)
        "ReadAHead",                                        // Hrvatski (Croatian)
        "ReadAHead",                                        // Eesti
        "Læs forud",                                        // Dansk
#if VDRVERSNUM >= 10342
        "Dopøedné ètení",                                   // Czech
#endif
    },
    {
     "Setup.DVD$Gain (analog)",
     "Verstärkung (analog)",                                // Deutsch
     "Ojaèanje (analogno)",                                 // Slovenski
     "Gain (analog)",                                       // Italiano
     "Versterking (analoog)",                               // Nederlands
     "Gain (analog)",                                       // Português
     "Gain (analog)",                                       // Français
     "Gain (analog)",                                       // Norsk
     "Äänen vahvistus (analoginen)",                        // suomi
     "Zysk (analogowo)",                                    // Polski
     "Ganancia (analógico)",                                // Español
     "Gain (analogika)",                                    // ÅëëçíéêÜ (Greek)
     "Gain (analog)",                                       // Svenska
     "Gain (analog)",                                       // Romaneste
     "Gain (analog)",                                       // Magyar
     "Gain (analog)",                                       // Català
     "ÃáØÛÕÝØÕ (ÐÝÐÛÞÓ.)",                                  // ÀãááÚØÙ (Russian)
     "Gain (analog)",                                       // Hrvatski (Croatian)
     "Helivõimendamine (analoog)",                          // Eesti
     "Forstærkning (analog)",                               // Dansk
#if VDRVERSNUM >= 10342
     "Zesílení (analog)",                                   // Czech
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
        "¾èØÑÚÐ ÞâÚàëâØï DVD!",                             // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "DVD avamine ebaõnnestus!",                         // Eesti
        "Fejl ved åbning af DVD!",                          // Dansk
#if VDRVERSNUM >= 10342
        "Chyba pøi otevírání DVD!",                         // Czech
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
        "¾èØÑÚÐ çâÕÝØï DVD!",                               // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Lugemine DVD-lt ebaõnnestus!",                     // Eesti
        "Fejl ved hentning af data fra DVD!",               // Dansk
#if VDRVERSNUM >= 10342
        "Chyba pøi naèítání dat z DVD!",                    // Czech
#endif
    },
    {
    "Error.DVD$Current subp stream not seen!",              // English
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
        "½ÕÔÞáâãßÕÝ ÒØÔÕÞßÞâÞÚ âÕÚãéÕÓÞ àÐ×ÔÕÛÐ!",          // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitriterada pole avastatud!",                  // Eesti
        "Valgte undertekst findes ikke!",                   // Dansk
#if VDRVERSNUM >= 10342
        "Vybrané titulky nejsou k dispozici!",              // Czech
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
        "½ÕÔÞáâãßÕÝ âÕÚãéØÙ ÐãÔØÞßÞâÞÚ!",                   // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Audiorada pole avastatud!",                        // Eesti
        "Valgte lyd-spor findes ikke!",                     // Dansk
#if VDRVERSNUM >= 10342
        "Vybraná zvuková stopa není k dispozici!",          // Czech
#endif
    },
    { NULL }
};
