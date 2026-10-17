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
    "sv",
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
  // Language ISO 639-1 codes for DVD and the language names, UTF-8
    {"en", "English"},
    {"de", "Deutsch"},
    {"sl", "Slovenski"},
    {"it", "Italiano"},
    {"nl", "Nederlands"},
    {"pt", "Portugu√™s"},
    {"fr", "Fran√ßais"},
    {"no", "Norsk"},
    {"fi", "suomi"},
    {"pl", "Polski"},
    {"es", "Espa√±ol"},
    {"el", "ŒïŒªŒªŒ∑ŒΩŒπŒ∫Œ¨"},
    {"sv", "Svenska"},
    {"ro", "Romaneste"},
    {"hu", "Magyar"},
    {"ca", "Catal√†"},
    {"ru", "–†—É—Å—Å–∫–∏–π"},
    {"hr", "Hrvatski"},
    {"et", "Eesti"},
    {"da", "Dansk"},
    {"cs", "Czech"},
    {"tr", "T√ºrk√ße"}
};

#if VDRVERSNUM < 10507
const tI18nPhrase DvdPhrases[] = {
    {
    "Plugin.DVD$DVD",                                       // English
        "DVD",                                              // Deutsch
        "DVD",                                              // Slovenski
        "DVD",                                              // Italiano
        "DVD",                                              // Nederlands
        "DVD",                                              // PortuguÍs
        "DVD",                                              // FranÁais
        "DVD",                                              // Norsk
        "DVD",                                              // suomi
        "DVD",                                              // Polski
        "DVD",                                              // EspaÒol
        "DVD",                                              // ≈ÎÎÁÌÈÍ‹ (Greek)
        "DVD",                                              // Svenska
        "DVD",                                              // Romaneste
        "DVD",                                              // Magyar
        "DVD",                                              // Catal‡
        "ø‡ﬁÿ”‡Î“–‚’€Ï DVD",                                // ¿„··⁄ÿŸ (Russian)
        "DVD",                                              // Hrvatski (Croatian)
        "DVD",                                              // Eesti
        "DVD",                                              // Dansk
        "DVD",                                              // Czech
#if VDRVERSNUM >= 10502
        "DVD",                                              // T¸rkÁe
#endif
    },
    {
    "Plugin.DVD$turn VDR into an (almost) full featured DVD player",    // English
        "verwandelt VDR in einen (fast) vollst‰ndigen DVD Spieler",     // Deutsch
        "spremeni VDR v (skoraj) popolen DVD predvajalnik",             // Slovenski
        "",                                                             // Italiano
        "Verander VDR in een (bijna) complete DVD-speler",              // Nederlands
        "",                                                             // PortuguÍs
        "",                                                             // FranÁais
        "",                                                             // Norsk
        "DVD-soitin",                                                   // suomi
        "Zmienia VDR w odtwarzacz DVD",                                 // Polski
        "Convierte VDR en un reproductor DVD (casi) completo",          // EspaÒol
        "Metatropi tou VDR se ena (sxedon) olokliromeno DVD",           // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                             // Svenska
        "",                                                             // Romaneste
        "",                                                             // Magyar
        "",                                                             // Catal‡
        "ø‡ﬁÿ”‡Î“–‚’€Ï DVD",                                            // ¿„··⁄ÿŸ (Russian)
        "",                                                             // Hrvatski (Croatian)
        "DVD-m‰ngija",                                                  // Eesti
        "forvandler VDR til en (nÊsten) normal DVD afspiller",          // Dansk
        "P¯emÏnÌ VDR v plnohodnotn˝ DVD p¯ehr·vaË",                     // Czech
#if VDRVERSNUM >= 10502
        "VDR'i (nerdeyse) tam DVD oynat˝c˝s˝na Áevirir"                 // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$Preferred menu language",                    // English
        "Bevorzugte Sprache f¸r Men¸s",                     // Deutsch
        "Prednostni jezik za menije",                       // Slovenski
        "menu - linguaggio preferito",                      // Italiano
        "Taalkeuze voor menu",                              // Nederlands
        "",                                                 // PortuguÍs
        "Langage prÈfÈrÈ pour les menus",                   // FranÁais
        "",                                                 // Norsk
        "Haluttu valikkokieli",                             // suomi
        "Preferowany jÍzyk menu",                           // Polski
        "Idioma preferido para los men˙s",                  // EspaÒol
        "Protinomeni glossa gia menou",                     // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "ø‡’‘ﬂﬁÁÿ‚–’‹ÎŸ Ô◊Î⁄ ‹’›Ó",                         // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud men¸¸keel",                             // Eesti
        "Foretrukket sprog for menuer",                     // Dansk
        "Preferovan˝ jazyk menu",                           // Czech
#if VDRVERSNUM >= 10502
        "Men¸lerin dil tercihi"                             // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$Preferred audio language",                   // English
        "Bevorzugte Sprache f¸r Dialog",                    // Deutsch
        "Prednostni jezik za zvok",                         // Slovenski
        "audio - linguaggio preferito",                     // Italiano
        "Taalkeuze voor geluid",                            // Nederlands
        "",                                                 // PortuguÍs
        "Langage prÈfÈrÈ pour le son",                      // FranÁais
        "",                                                 // Norsk
        "Haluttu ‰‰niraita",                                // suomi
        "Preferowany jÍzyk dºwiÍku",                        // Polski
        "Idioma preferido para el sonido",                  // EspaÒol
        "Protinomeni glossa gia ton dialogo",               // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "ø‡’‘ﬂﬁÁÿ‚–’‹ÎŸ Ô◊Î⁄ –„‘ÿﬁ",                        // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud audiokeel",                             // Eesti
        "Foretrukket sprog for lyd",                        // Dansk
        "Preferovan˝ jazyk zvuku",                          // Czech
#if VDRVERSNUM >= 10502
        "Diyaloglar˝n dil tercihi"                          // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$Preferred subtitle language",                // English
        "Bevorzugte Sprache f¸r Untertitel",                // Deutsch
        "Prednostni jezik za podnapise",                    // Slovenski
        "sottotitoli - linguaggio preferito",               // Italiano
        "Taalkeuze voor ondertitels",                       // Nederlands
        "",                                                 // PortuguÍs
        "Langage prÈfÈrÈ pour les sous-titres",             // FranÁais
        "",                                                 // Norsk
        "Haluttu tekstityskieli",                           // suomi
        "Preferowany jÍzyk napisÛw",                        // Polski
        "Idioma preferido para los subtÌtulos",             // EspaÒol
        "Protinomeni glossa ipotitlon",                     // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "ø‡’‘ﬂﬁÁÿ‚–’‹ÎŸ Ô◊Î⁄ ·„—‚ÿ‚‡ﬁ“",                    // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Eelistatud subtiitritekeel",                       // Eesti
        "Foretrukket sprog for undertekster",               // Dansk
        "Preferovan˝ jazyk titulk˘",                        // Czech
#if VDRVERSNUM >= 10502
        "Altyaz˝lar˝n dil tercihi"                          // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$Player region code",                         // English
        "Regionalkode f¸r DVD Spieler",                     // Deutsch
        "Regionalna koda za predvajalnik",                  // Slovenski
        "region code del DVD player",                       // Italiano
        "Regiocode van Speler",                             // Nederlands
        "",                                                 // PortuguÍs
        "Code rÈgion du lecteur",                           // FranÁais
        "",                                                 // Norsk
        "Soittimen aluekoodi",                              // suomi
        "Kod regionu odtwarzacza",                          // Polski
        "CÛdigo de zona del lector",                        // EspaÒol
        "Kodikos Zonis DVD",                                // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "∫ﬁ‘ ◊ﬁ›Î",                                         // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "M‰ngija regioonikood",                             // Eesti
        "Afspillerens regions kode",                        // Dansk
        "Region·lnÌ kÛd p¯ehr·vaËe",                        // Czech
#if VDRVERSNUM >= 10502
        "DVD oynat˝c˝n˝n bˆlge kodu"                        // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$Display subtitles",                          // English
        "Untertitel anzeigen",                              // Deutsch
        "Prikaæi podnapise",                                // Slovenski
        "Visualizza sottotitoli",                           // Italiano
        "Toon ondertitels",                                 // Nederlands
        "",                                                 // PortuguÍs
        "Affiche les sous-titres",                          // FranÁais
        "",                                                 // Norsk
        "N‰yt‰ tekstitys",                                  // suomi
        "Wy∂wietlaj napisy",                                // Polski
        "Mostrar subtÌtulos",                               // EspaÒol
        "Endiksi ipotitlon",                                // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "øﬁ⁄–◊Î“–‚Ï ·„—‚ÿ‚‡Î",                              // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitrite kuvamine",                             // Eesti
        "Vis undertekster",                                 // Dansk
        "Zobrazovat titulky",                               // Czech
#if VDRVERSNUM >= 10502
        "Altyaz˝lar˝ gˆster"                                // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$Hide Mainmenu Entry",
        "Hauptmen¸eintrag verstecken",                      // Deutsch
        "Skrij galvni meni",                                // Slovenski
        "Nascondi voce men˘",                               // Italiano
        "Verberg vermelding in het hoofdmenu",              // Nederlands
        "",                                                 // PortuguÍs
        "",                                                 // FranÁais
        "",                                                 // Norsk
        "Piilota valinta p‰‰valikosta",                     // suomi
        "Ukryj pozycjÍ w g≥Ûwnym menu",                     // Polski
        "Ocultar entrada en el men˙ principal",             // EspaÒol
        "Apokripsi sto vasiko menou",                       // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "¡⁄‡Î‚Ï ⁄ﬁ‹–›‘„ “ ”€–“›ﬁ‹ ‹’›Ó",                    // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Peita peamen¸¸ valikust",                          // Eesti
        "Skjul DVD i hovedmenuen",                          // Dansk
        "Skr˝t poloæku v hlavnÌm menu",                     // Czech
#if VDRVERSNUM >= 10502
        "Ana men¸de sakla"                                  // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$ReadAHead",
        "ReadAHead",                                        // Deutsch
        "Beri vnaprej",                                       // Slovenski
        "ReadAHead",                                        // Italiano
        "Vooruit lezen",                                    // Nederlands
        "ReadAHead",                                        // PortuguÍs
        "ReadAHead",                                        // FranÁais
        "ReadAHead",                                        // Norsk
        "ReadAHead-toiminto",                               // suomi
        "ReadAHead",                                        // Polski
        "Lectura anticipada",                               // EspaÒol
        "ReadAHead",                                        // ≈ÎÎÁÌÈÍ‹ (Greek)
        "ReadAHead",                                        // Svenska
        "ReadAHead",                                        // Romaneste
        "ReadAHead",                                        // Magyar
        "ReadAHead",                                        // Catal‡
        "√ﬂ‡’÷‘–ÓÈ’’ Á‚’›ÿ’",                               // ¿„··⁄ÿŸ (Russian)
        "ReadAHead",                                        // Hrvatski (Croatian)
        "ReadAHead",                                        // Eesti
        "LÊs forud",                                        // Dansk
        "Dop¯ednÈ ËtenÌ",                                   // Czech
#if VDRVERSNUM >= 10502
        "›leri oku"                                         // T¸rkÁe
#endif
    },
    {
    "Setup.DVD$Gain (analog)",
        "Verst‰rkung (analog)",                             // Deutsch
        "OjaËanje (analogno)",                              // Slovenski
        "Gain (analog)",                                    // Italiano
        "Versterking (analoog)",                            // Nederlands
        "Gain (analog)",                                    // PortuguÍs
        "Gain (analog)",                                    // FranÁais
        "Gain (analog)",                                    // Norsk
        "ƒ‰nen vahvistus (analoginen)",                     // suomi
        "Zysk (analogowo)",                                 // Polski
        "Ganancia (analÛgico)",                             // EspaÒol
        "Gain (analogika)",                                 // ≈ÎÎÁÌÈÍ‹ (Greek)
        "Gain (analog)",                                    // Svenska
        "Gain (analog)",                                    // Romaneste
        "Gain (analog)",                                    // Magyar
        "Gain (analog)",                                    // Catal‡
        "√·ÿ€’›ÿ’ (–›–€ﬁ”.)",                               // ¿„··⁄ÿŸ (Russian)
        "Gain (analog)",                                    // Hrvatski (Croatian)
        "Helivıimendamine (analoog)",                       // Eesti
        "ForstÊrkning (analog)",                            // Dansk
        "ZesÌlenÌ (analog)",                                // Czech
#if VDRVERSNUM >= 10502
        "Kuvvetlendirmek (analog)"                          // T¸rkÁe
#endif
    },
    {
    "Error.DVD$Error opening DVD!",                         // English
        "Fehler beim ÷ffnen der DVD!",                      // Deutsch
        "Napaka pri odpiranju DVD-ja!",                     // Slovenski
        "",                                                 // Italiano
        "Fout bij het openen van de DVD!",                  // Nederlands
        "",                                                 // PortuguÍs
        "",                                                 // FranÁais
        "",                                                 // Norsk
        "DVD:n avaaminen ep‰onnistui!",                     // suomi
        "B≥±d otwierania DVD!",                             // Polski
        "°Error abriendo el DVD!",                          // EspaÒol
        "Lathos sto anigma tou DVD",                        // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "æËÿ—⁄– ﬁ‚⁄‡Î‚ÿÔ DVD!",                             // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "DVD avamine ebaınnestus!",                         // Eesti
        "Fejl ved Âbning af DVD!",                          // Dansk
        "Chyba p¯i otevÌr·nÌ DVD!",                         // Czech
#if VDRVERSNUM >= 10502
        "DVD aÁ˝l˝˛ hatas˝!"                                // T¸rkÁe
#endif
    },
    {
    "Error.DVD$Error fetching data from DVD!",              // English
        "Fehler beim Lesen von der DVD!",                   // Deutsch
        "Napaka pri branju podatkov iz DVD-ja!",            // Slovenski
        "",                                                 // Italiano
        "Error bij het verkrijgen van data van de DVD!",    // Nederlands
        "",                                                 // PortuguÍs
        "",                                                 // FranÁais
        "",                                                 // Norsk
        "DVD:n lukeminen ep‰onnistui",                      // suomi
        "B≥±d pobierania danych z DVD!",                    // Polski
        "°Error obteniendo datos del DVD!",                 // EspaÒol
        "Lathos sto diavasma tou DVD",                      // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "æËÿ—⁄– Á‚’›ÿÔ DVD!",                               // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Lugemine DVD-lt ebaınnestus!",                     // Eesti
        "Fejl ved hentning af data fra DVD!",               // Dansk
        "Chyba p¯i naËÌt·nÌ dat z DVD!",                    // Czech
#if VDRVERSNUM >= 10502
        "DVD okuma hatas˝!"                                 // T¸rkÁe
#endif
    },
    {
    "Error.DVD$Current subtitle stream not seen!",          // English
        "Der ausgew‰hlte Untertitel ist nicht vorhanden!",  // Deutsch
        "Izbrani podnapisi niso prisotni!",                 // Slovenski
        "",                                                 // Italiano
        "De gekozen ondertiteling is niet beschikbaar!",    // Nederlands
        "",                                                 // PortuguÍs
        "",                                                 // FranÁais
        "",                                                 // Norsk
        "Tekstitysraitaa ei havaita!",                      // suomi
        "Wybrane napisy nie istniej±!",                     // Polski
        "°SubtÌtulos actuales no encontrados!",             // EspaÒol
        "O epilegmenos ipotitlos den iparxi",               // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "Ω’‘ﬁ·‚„ﬂ’› “ÿ‘’ﬁﬂﬁ‚ﬁ⁄ ‚’⁄„È’”ﬁ ‡–◊‘’€–!",          // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Subtiitriterada pole avastatud!",                  // Eesti
        "Valgte undertekst findes ikke!",                   // Dansk
        "VybranÈ titulky nejsou k dispozici!",              // Czech
#if VDRVERSNUM >= 10502
        "SeÁilen altyaz˝ bulunamad˝!"                       // T¸rkÁe
#endif
    },
    {
    "Error.DVD$Current audio track not seen!",              // English
        "Die ausgew‰hlte Audiospur ist nicht vorhanden!",   // Deutsch
        "Izbrani zvoËni zapis ni prisoten!",                // Slovenski
        "",                                                 // Italiano
        "Het gekozen audiospoort is niet beschikbaar",      // Nederlands
        "",                                                 // PortuguÍs
        "",                                                 // FranÁais
        "",                                                 // Norsk
        "ƒ‰niraitaa ei havaita!",                           // suomi
        "Wybrana ∂cieøka dºwiÍkowa nie istnieje!",          // Polski
        "°Pista de audio actual no encontrada!",            // EspaÒol
        "I Epilegmeni desmi ixou den mpori na wrethi",      // ≈ÎÎÁÌÈÍ‹ (Greek)
        "",                                                 // Svenska
        "",                                                 // Romaneste
        "",                                                 // Magyar
        "",                                                 // Catal‡
        "Ω’‘ﬁ·‚„ﬂ’› ‚’⁄„ÈÿŸ –„‘ÿﬁﬂﬁ‚ﬁ⁄!",                   // ¿„··⁄ÿŸ (Russian)
        "",                                                 // Hrvatski (Croatian)
        "Audiorada pole avastatud!",                        // Eesti
        "Valgte lyd-spor findes ikke!",                     // Dansk
        "Vybran· zvukov· stopa nenÌ k dispozici!",          // Czech
#if VDRVERSNUM >= 10502
        "SeÁilen audio ses bulunamad˝!"                     // T¸rkÁe
#endif
    },
    { NULL }
};
#endif
