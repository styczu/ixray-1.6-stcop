#pragma once

// Strony kodowe uzywane przez lokalizacje X-Ray.
//
// Pliki gamedata/configs/text/<lang>/*.xml sa jednobajtowe, a ich strona
// kodowa zalezy od jezyka - mimo ze naglowek XML wszedzie deklaruje
// windows-1251. Wartosci enuma sa numerami stron kodowych Windows, wiec
// mozna je przekazac wprost do MultiByteToWideChar / WideCharToMultiByte.
enum class ECodepage
{
	CP1250 = 1250,	// srodkowoeuropejska
	CP1251 = 1251,	// cyrylica
	CP1252 = 1252,	// zachodnioeuropejska
};

namespace Localization
{
	// Strona kodowa dla kodu jezyka (nazwa katalogu w configs/text/).
	// Nieznany jezyk -> CP1251, czyli dotychczasowe zachowanie silnika.
	XRCORE_API ECodepage CodepageForLanguage(const char* Language);

	// Tlumaczy bajt tekstu na codepoint Unicode wedlug strony kodowej.
	// Zwraca 0 dla pozycji niezdefiniowanych w danej stronie kodowej.
	XRCORE_API wchar_t TranslateSymbol(char Symbol, ECodepage Codepage);

	// Konwersja jednobajtowego TEKSTU LOKALIZACJI na UTF-8.
	//
	// Wylacznie dla tekstow wyswietlanych - NIE dla sciezek plikow.
	// Sciezki musza isc przez Platform::ANSI_TO_UTF8 ze stalym CP1251,
	// bo nazwy plikow nie zmieniaja sie wraz z jezykiem interfejsu.
	//
	// Wlasna implementacja zamiast WideCharToMultiByte, zeby dzialalo
	// tez poza Windows.
	XRCORE_API xr_string AnsiToUtf8(const xr_string& Ansi, ECodepage Codepage);

	// Strona kodowa aktualnie wybranego jezyka. Ustawiana przez CStringTable
	// przy inicjalizacji i przeladowaniu jezyka; xrCore nie moze sam odpytac
	// tablicy stringow, bo ta zyje w xrEngine.
	XRCORE_API ECodepage GetActiveCodepage();
	XRCORE_API void SetActiveCodepage(ECodepage Codepage);
	XRCORE_API void SetActiveCodepageForLanguage(const char* Language);
}
