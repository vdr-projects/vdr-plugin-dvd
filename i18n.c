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
#if VDRVERSNUM > 10312
    "et",
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
        "DVD",                                              // ÀãááÚØÙ (Russian)
        "DVD",                                              // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "DVD",                                              // Eesti
#endif
    },
    {
    "Plugin.DVD$turn VDR into an (almost) full featured DVD player",    // English
        "",                                                             // Deutsch
        "",                                                             // Slovenski
        "",                                                             // Italiano
        "",                                                             // Nederlands
        "",                                                             // Português
        "",                                                             // Français
        "",                                                             // Norsk
        "DVD-soitin",                                                   // suomi
        "",                                                             // Polski
        "",                                                             // Español
        "",                                                             // ÅëëçíéêÜ (Greek)
        "",                                                             // Svenska
        "",                                                             // Romaneste
        "",                                                             // Magyar
        "",                                                             // Català
        "",                                                             // ÀãááÚØÙ (Russian)
        "",                                                             // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                             // Eesti
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
        "",                                                 // Polski
        "Idioma preferido para los menús",                  // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "",                                                 // Polski
        "Idioma preferido para el sonido",                  // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "",                                                 // Polski
        "Idioma preferido para los subtítulos",             // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
#endif
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
        "Soittimen aluekoodi",                              // suomi
        "",                                                 // Polski
        "Código de zona del lector",                        // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "",                                                 // Polski
        "Mostrar subtítulos",                               // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
#endif
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
        "Piilota valinta päävalikosta",                     // suomi
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "ÁÚàëâì ÚÞÜÐÝÔã Ò ÓÛÐÒÝÞÜ ÜÕÝî",                    // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "ReadAHead",                                        // Español
        "ReadAHead",                                        // ÅëëçíéêÜ (Greek)
        "ReadAHead",                                        // Svenska
        "ReadAHead",                                        // Romaneste
        "ReadAHead",                                        // Magyar
        "ReadAHead",                                        // Català
        "ReadAHead",                                        // ÀãááÚØÙ (Russian)
        "ReadAHead",                                        // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
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
        "",                                                 // Polski
        "",                                                 // Español
        "",                                                 // ÅëëçíéêÜ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Català
        "",                                                 // ÀãááÚØÙ (Russian)
        "",                                                 // Hrvatski (Croatian)
#if VDRVERSNUM > 10312
        "",                                                 // Eesti
#endif
    },
    { NULL }
};
