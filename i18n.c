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
    "",
};

const tI18nPhrase DvdPhrases[] = {
    {
    "Setup.DVD$Preferred menu language",                    // English
    "Bevorzugte Spache für Menüs",                          // Deutsch
        "",                                                 // Slovenski
        "menu - linguaggio preferito",                      // Italiano
        "Taalkeuze voor menu",                              // Nederlands
        "",                                                 // Português
        "Langage préféré pour les menus",                   // Français
        "",                                                 // Norsk
        "Haluttu valikon kieli",                            // Suomi
        "",                                                 // Polski
        "Idioma preferido para los menús",                  // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
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
        "Haluttu äänityksen kieli",                         // Suomi
        "",                                                 // Polski
        "Idioma preferido para el sonido",                  // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
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
        "Haluttu tekstityksen kieli",                       // Suomi
        "",                                                 // Polski
        "Idioma preferido para los subtítulos",             // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
    },
    {
    "Setup.DVD$Player region code",                         // English
        "Regions Kode für Player",                          // Deutsch
        "",                                                 // Slovenski
        "region code del DVD player",                       // Italiano
        "Regiocode van Speler",                             // Nederlands
        "",                                                 // Português
        "Code région du lecteur",                           // Français
        "",                                                 // Norsk
        "Soittimen aluekoodi",                              // Suomi
        "",                                                 // Polski
        "Código de zona del lector",                        // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
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
        "Näytä tekstitys",                                  // Suomi
        "",                                                 // Polski
        "Mostrar subtítulos",                               // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
    },
    {
     "Setup.DVD$Hide Mainmenu Entry",
        "Hauptmenüeintrag verstecken"                       // Deutsch
        "",                                                 // Slovenski
        "Nascondi voce menù",                               // Italiano
        "",                                                 // Nederlands
        "",                                                 // Português
        "",                                                 // Français
        "",                                                 // Norsk
        "Piilota valinta päävalikosta",                     // Suomi
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "ÁÚàëâì ÚÞÜÐÝÔã Ò ÓÛÐÒÝÞÜ ÜÕÝî",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
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
        "",                                                 // Suomi
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
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
        "",                                                 // Suomi
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
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
        "",                                                 // Suomi
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
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
        "",                                                 // Suomi
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // Ellinika
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
    },
    { NULL }
};
