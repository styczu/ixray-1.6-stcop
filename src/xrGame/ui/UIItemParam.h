#pragma once
#include "../../xrUI/Widgets/UIStatic.h"

class CUIXml;

class UIItemParam : public CUIStatic
{
public:
	UIItemParam();
	~UIItemParam() override;

	enum class InitResult
	{
		Failed,
		Normal,
		Plain
	};

	InitResult Init(CUIXml& Xml, const char* Section);

	void SetDefaultValuesPlain(float Magnitude, bool IsInverse, const shared_str& UnitValue);
	void SetCaption(const char* Name);
	void SetValue(float Value, bool ApplyMagnitude = true);

	void SetValueText(const char* Text);
	void ShowValue(bool IsShown);

	// Wartosci z jednostka (ui-param-bars): wspolczynnik absorpcji artefaktu x100,
	// tempo radiacji i regeneracji na sekunde czasu rzeczywistego, czas dzialania.
	void SetAbsorption(float Immunity);
	void SetRadiationRate(float RatePerGameSecond);
	void SetRegenerationRate(float RatePerGameSecond);
	void SetDuration(float GameSeconds);
	void Update() override;

	CUIWindow* ui_cast_window() override { return this; }
	bool GetLegacyMode() const { return !(Caption && Caption->IsShown() && ValueText && ValueText->IsShown()); }

	float m_magnitude;

protected:
	InitResult InitPlain(CUIXml& Xml, const char* Section);
	void ApplyValueText(const char* Text, bool IsPositive, u32 LegacyColor);

private:
	CUIStatic* Caption;
	CUIStatic* ValueText;
	CUIStatic* LegacyTextWidget;
	bool IsSignInverse;
	bool ShowSign = true;
	bool ColorizeValue = false;
	bool ColorizeText = false;
	shared_str Unit;
	shared_str NegativeTexture;
	shared_str PositiveTexture;

	// Tempo i czas zaleza od przelicznika czasu gry, wiec Update odswieza tekst.
	float RegenerationRate = 0.0f;
	bool HasRegenerationRate = false;
	float DurationSeconds = 0.0f;
	bool HasDuration = false;

}; // class UIItemParam
