/*
 * DVD Player plugin for VDR
 *
 * Copyright (C) 2001.2002 Andreas Schultz <aschultz@warp10.net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef __QNXNTO__
#include "i18n.h"
#else
#include "vdr/i18n.h"
#endif

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
#if VDRVERSNUM>=10300
    "ru",
#endif
#if VDRVERSNUM>=10307
    "",
#endif
};

const tI18nPhrase DvdPhrases[] = {
    {
    "Setup.DVD$Preferred menu language",                // English
    "Bevorzugte Spache für Menüs",                      // Deutsch
        "", // Slovenski
        "menu - linguaggio preferito",                  // Italiano
        "Taalkeuze voor menu",                          // Nederlands
        "",                                             // Português
        "Langage préféré pour les menus",               // Français
        "",                                             // Norsk
        "Haluttu valikon kieli",                        // Suomi
        "",                                             // Polski
        "Idioma preferido para los menús",              // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "",                                             // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
    "Setup.DVD$Preferred audio language",               // English
        "Bevorzugte Sprache für Dialog",                // Deutsch
        "",                                             // Slovenski
        "audio - linguaggio preferito",                 // Italiano
        "Taalkeuze voor geluid",                        // Nederlands
        "",                                             // Português
        "Langage préféré pour le son",                  // Français
        "",                                             // Norsk
        "Haluttu äänityksen kieli",                     // Suomi
        "",                                             // Polski
        "Idioma preferido para el sonido",              // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "",                                             // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
    "Setup.DVD$Preferred subtitle language",            // English
        "Bevorzugte Spache für Untertitel",             // Deutsch
        "",                                             // Slovenski
        "sottotitoli - linguaggio preferito",           // Italiano
        "Taalkeuze voor ondertitels",                   // Nederlands
        "",                                             // Português
        "Langage préféré pour les sous-titres",         // Français
        "",                                             // Norsk
        "Haluttu tekstityksen kieli",                   // Suomi
        "",                                             // Polski
        "Idioma preferido para los subtítulos",         // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "",                                             // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
    "Setup.DVD$Player region code",                     // English
        "Regions Kode für Player",                      // Deutsch
        "",                                             // Slovenski
        "region code del DVD player",                   // Italiano
        "Regiocode van Speler",                         // Nederlands
        "",                                             // Português
        "Code région du lecteur",                       // Français
        "",                                             // Norsk
        "Soittimen aluekoodi",                          // Suomi
        "",                                             // Polski
        "Código de zona del lector",                    // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "",                                             // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
    "Setup.DVD$Display subtitles",                      // English
        "Untertitel anzeigen",                          // Deutsch
        "",                                             // Slovenski
        "Visualizza sottotitoli",                       // Italiano
        "Toon ondertitels",                             // Nederlands
        "",                                             // Português
        "Affiche les sous-titres",                      // Français
        "",                                             // Norsk
        "Näytä tekstitys",                              // Suomi
        "",                                             // Polski
        "Mostrar subtítulos",                           // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "",                                             // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
    "Setup.DVD$forced only",                            // English
        "nur geforderte"                                // Deutsch
        "",                                             // Slovenski
        "",                                             // Italiano
        "",                                             // Nederlands
        "",                                             // Português
        "",                                             // Français
        "",                                             // Norsk
        "",                                             // Suomi
        "",                                             // Polski
        "",                                             // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "",                                             // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
     "Setup.DVD$Hide Mainmenu Entry",
        "Hauptmenüeintrag verstecken"                   // Deutsch
        "",                                             // Slovenski
        "Nascondi voce menù",                           // Italiano
        "",                                             // Nederlands
        "",                                             // Português
        "",                                             // Français
        "",                                             // Norsk
        "Piilota valinta päävalikosta",                 // Suomi
        "",                                             // Polski
        "",                                             // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "ÁÚàëâì ÚÞÜÐÝÔã Ò ÓÛÐÒÝÞÜ ÜÕÝî",                // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
    "Error.DVD$Error opening DVD!",                     // English
        "Fehler beim öffnen der DVD!",                  // Deutsch
        "",                                             // Slovenski
        "",                                             // Italiano
        "",                                             // Nederlands
        "",                                             // Português
        "",                                             // Français
        "",                                             // Norsk
        "",                                             // Suomi
        "",                                             // Polski
        "",                                             // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
        "",                                             // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    {
    "Error.DVD$Error fetching data from DVD!",          // English
        "Fehler beim lesen von der DVD!",               // Deutsch
        "",                                             // Slovenski
        "",                                             // Italiano
        "",                                             // Nederlands
        "",                                             // Português
        "",                                             // Français
        "",                                             // Norsk
        "",                                             // Suomi
        "",                                             // Polski
        "",                                             // Español
        "",                                             // Ellinika
        "",                                             // Svenska
        "",                                             // Romaneste
        "",
        "",
#if VDRVERSNUM>=10300
      "",                                               // Russian
#endif
#if VDRVERSNUM>=10307
        "",
#endif
    },
    { NULL }
};
