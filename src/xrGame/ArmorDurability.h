#pragma once

#include <cmath>

// Skala trwalosci pancerza (kombinezon, helm). Wspolczynnik zuzycia k to
// klucz *_immunity sekcji immunities_sect przedmiotu: trafienie o sile P
// zabiera P * k stanu. Punkty S rosna logarytmicznie z liczba trafien do
// zuzycia (1 / k): k = 0,05 -> 2 pkt, k = 0,001 -> 100 pkt, wiec ten sam
// przyrost punktow zawsze oznacza ten sam mnoznik zuzycia (17 pkt ~ 2x mniej).
// Ulepszenie "durability_points = N" dodaje N punktow kazdemu typowi trafienia,
// czyli mnozy wszystkie k przez Factor(N). Typ z k = 0 nie zuzywa przedmiotu
// i jest poza skala.
namespace ArmorDurability
{
	constexpr float WorstWear = 0.05f;
	constexpr float BestWear = 0.001f;
	constexpr float WorstPoints = 2.0f;
	constexpr float BestPoints = 100.0f;

	inline float Factor(float Points)
	{
		return std::pow(WorstWear / BestWear, -Points / (BestPoints - WorstPoints));
	}

	inline float Points(float Wear)
	{
		return WorstPoints + (BestPoints - WorstPoints) * std::log(WorstWear / Wear) / std::log(WorstWear / BestWear);
	}
}
