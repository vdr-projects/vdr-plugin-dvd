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
        "",                                                             // Slovenski
        "",                                                             // Italiano
        "",                                                             // Nederlands
        "",                                                             // Português
        "",                                                             // Français
        "",                                                             // Norsk
        "DVD-soitin",                                                   // suomi
        "Zmienia VDR w odtwarzacz DVD",                                 // Polski
        "Convierte VDR en un reproductor DVD (casi) completo",          // Español
        "",                                                             // ÅëëçíéêÜ (Greek)
        "",                                                             // Svenska
        "",                                                             // Romaneste
        "",                                                             // Magyar
        "",                                                             // Català
        "¿àÞØÓàëÒÐâÕÛì DVD",                                            // ÀãááÚØÙ (Russian)
        "",                                                             // Hrvatski (Croatian)
        "",                                                             // Eesti
        "forvandler VDR til en (næsten) normal DVD afspiller",          // Dansk
#if VDRVERSNUM >= 10342
        "",                                                             // Czech
#endif
    },
    {
    "Setup.DVD$Preferred menu language",                    // English
        "Bevorzugte Spache für Menüs",                      // Deutsch
        "",                                                 // Slovenski
        "menu - linguaggio preferito",                      // Italiano
        "Taalkeuze voor menu",                              // Nederlands
        "",                                                 // Português
        "Langage préféré pour les menus",                   // Français
        "",                                                 // Norsk
        "Haluttu valikkokieli",                             // suomi
        "Preferowany jêzyk menu",                           // Polski
        "Idioma preferido para los menús",                  // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿àÕÔßÞçØâÐÕÜëÙ ï×ëÚ ÜÕÝî",                         // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Foretrukket sprog for menuer",                     // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
    "Setup.DVD$Preferred audio language",                   // English
        "Bevorzugte Sprache für Dialog",                    // Deutsch
        "",                                                 // Slovenski
        "audio - linguaggio preferito",                     // Italiano
        "Taalkeuze voor geluid",                            // Nederlands
        "",                                                 // Português
        "Langage préféré pour le son",                      // Français
        "",                                                 // Norsk
        "Haluttu ääniraita",                                // suomi
        "Preferowany jêzyk d¼wiêku",                        // Polski
        "Idioma preferido para el sonido",                  // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿àÕÔßÞçØâÐÕÜëÙ ï×ëÚ ÐãÔØÞ",                        // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Foretrukket sprog for lyd",                        // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
    "Setup.DVD$Preferred subtitle language",                // English
        "Bevorzugte Spache für Untertitel",                 // Deutsch
        "",                                                 // Slovenski
        "sottotitoli - linguaggio preferito",               // Italiano
        "Taalkeuze voor ondertitels",                       // Nederlands
        "",                                                 // Português
        "Langage préféré pour les sous-titres",             // Français
        "",                                                 // Norsk
        "Haluttu tekstityskieli",                           // suomi
        "Preferowany jêzyk napisów",                        // Polski
        "Idioma preferido para los subtítulos",             // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿àÕÔßÞçØâÐÕÜëÙ ï×ëÚ áãÑâØâàÞÒ",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Foretrukket sprog for undertekster",               // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
    "Setup.DVD$Player region code",                         // English
        "Regionalkode für DVD Spieler",                     // Deutsch
        "",                                                 // Slovenski
        "region code del DVD player",                       // Italiano
        "Regiocode van Speler",                             // Nederlands
        "",                                                 // Português
        "Code région du lecteur",                           // Français
        "",                                                 // Norsk
        "Soittimen aluekoodi",                              // suomi
        "Kod regionu odtwarzacza",                          // Polski
        "Código de zona del lector",                        // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "ºÞÔ ×ÞÝë",                                         // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Afspillerens regions kode",                        // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
    "Setup.DVD$Display subtitles",                          // English
        "Untertitel anzeigen",                              // Deutsch
        "",                                                 // Slovenski
        "Visualizza sottotitoli",                           // Italiano
        "Toon ondertitels",                                 // Nederlands
        "",                                                 // Português
        "Affiche les sous-titres",                          // Français
        "",                                                 // Norsk
        "Näytä tekstitys",                                  // suomi
        "Wy¶wietlaj napisy",                                // Polski
        "Mostrar subtítulos",                               // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¿ÞÚÐ×ëÒÐâì áãÑâØâàë",                              // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Vis undertekster",                                 // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
     "Setup.DVD$Hide Mainmenu Entry",
        "Hauptmenüeintrag verstecken",                      // Deutsch
        "",                                                 // Slovenski
        "Nascondi voce menù",                               // Italiano
        "",                                                 // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "Piilota valinta päävalikosta",                     // suomi
        "Ukryj pozycjê w g³ównym menu",                     // Polski
        "Ocultar entrada en el menú principal",             // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "ÁÚàëâì ÚÞÜÐÝÔã Ò ÓÛÐÒÝÞÜ ÜÕÝî",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Skjul DVD i hovedmenuen",                          // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
     "Setup.DVD$ReadAHead",
        "ReadAHead",                                        // Deutsch
        "ReadAHead",                                        // Slovenski
        "ReadAHead",                                        // Italiano
        "ReadAHead",                                        // Nederlands
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
        "ReadAHead",                                        // Czech
#endif
    },
    {
     "Setup.DVD$Gain (analog)",
     "Verstärkung (analog)",                                // Deutsch
     "Gain (analog)",                                       // Slovenski
     "Gain (analog)",                                       // Italiano
     "Gain (analog)",                                       // Nederlands
     "Gain (analog)",                                       // Português
     "Gain (analog)",                                       // Français
     "Gain (analog)",                                       // Norsk
     "Äänen vahvistus (analoginen)",                        // suomi
     "Zysk (analogowo)",                                    // Polski
     "Ganancia (analógico)",                                // Español
     "Gain (analog)",                                       // ÅëëçíéêÜ (Greek)
     "Gain (analog)",                                       // Svenska
     "Gain (analog)",                                       // Romaneste
     "Gain (analog)",                                       // Magyar
     "Gain (analog)",                                       // Català
     "ÃáØÛÕÝØÕ (ÐÝÐÛÞÓ.)",                                  // ÀãááÚØÙ (Russian)
     "Gain (analog)",                                       // Hrvatski (Croatian)
     "Gain (analog)",                                       // Eesti
     "Forstærkning (analog)",                               // Dansk
#if VDRVERSNUM >= 10342
     "Gain (analog)",                                       // Czech
#endif
    },
    {
    "Error.DVD$Error opening DVD!",                         // English
        "Fehler beim öffnen der DVD!",                      // Deutsch
        "",                                                 // Slovenski
        "",                                                 // Italiano
        "",                                                 // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "DVD:n avaaminen epäonnistui!",                     // suomi
        "B³±d otwierania DVD!",                             // Polski
        "¡Error abriendo el DVD!",                          // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¾èØÑÚÐ ÞâÚàëâØï DVD!",                             // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Fejl ved åbning af DVD!",                          // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
    "Error.DVD$Error fetching data from DVD!",              // English
        "Fehler beim lesen von der DVD!",                   // Deutsch
        "",                                                 // Slovenski
        "",                                                 // Italiano
        "",                                                 // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "DVD:n lukeminen epäonnistui",                      // suomi
        "B³±d pobierania danych z DVD!",                    // Polski
        "¡Error obteniendo datos del DVD!",                 // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "¾èØÑÚÐ çâÕÝØï DVD!",                               // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Fejl ved hentning af data fra DVD!",               // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
    "Error.DVD$Current subp stream not seen!",              // English
        "Der ausgewählte Untertitel ist nicht vorhanden!",  // Deutsch
        "",                                                 // Slovenski
        "",                                                 // Italiano
        "",                                                 // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "Tekstitysraitaa ei havaita!",                      // suomi
        "Wybrane napisy nie istniej±!",                     // Polski
        "¡Subtítulos actuales no encontrados!",             // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "½ÕÔÞáâãßÕÝ ÒØÔÕÞßÞâÞÚ âÕÚãéÕÓÞ àÐ×ÔÕÛÐ!",          // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Valgte undertekst findes ikke!",                   // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    {
    "Error.DVD$Current audio track not seen!",              // English
        "Die ausgewählte Audiospur ist nicht vorhanden!",   // Deutsch
        "",                                                 // Slovenski
        "",                                                 // Italiano
        "",                                                 // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "Ääniraitaa ei havaita!",                           // suomi
        "Wybrana ¶cie¿ka d¼wiêkowa nie istnieje!",          // Polski
        "¡Pista de audio actual no encontrada!",            // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "½ÕÔÞáâãßÕÝ âÕÚãéØÙ ÐãÔØÞßÞâÞÚ!",                   // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "",                                                 // Eesti
        "Valgte lyd-spor findes ikke!",                     // Dansk
#if VDRVERSNUM >= 10342
        "",                                                 // Czech
#endif
    },
    { NULL }
};
