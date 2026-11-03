#include "stdafx.h"
#include "UIItemStateDisplay.h"
#include "UIProgressBar.h"
#include "UIStatic.h"

CUIItemStateDisplay::CUIItemStateDisplay()
	: _mode(EDisplayMode::Bar),
	  _value(0.0f),
	  _progressBar(nullptr),
	  PortionBar(nullptr),
	  PortionBarSegments(8),
	  PortionBarCut(0.5f),
	  _showBackground(true),
	  _percentFormat(EPercentFormat::Percent),
	  _fractionMax(100),
	  _portionCurrent(0),
	  _portionMax(1),
	  _hasPortionData(false),
	  _percentBackground(nullptr),
	  _percentText(nullptr),
	  _useTextColor(false),
	  _useMiddleTextColor(false),
	  _useTextGradient(true)
{
	Enable(false);
	m_bUseGradient = true;
	_minTextColor.set(1.0f, 1.0f, 1.0f, 1.0f);
	_middleTextColor.set(1.0f, 1.0f, 1.0f, 1.0f);
	_maxTextColor.set(1.0f, 1.0f, 1.0f, 1.0f);
}

CUIItemStateDisplay::~CUIItemStateDisplay()
{
	_progressBar = nullptr;
	PortionBar = nullptr;
	_percentBackground = nullptr;
	_percentText = nullptr;
}

void CUIItemStateDisplay::SetState(float state)
{
	_value = state;
	_hasPortionData = false;
	if (_mode == EDisplayMode::Percent)
	{
		clamp(_value, 0.0f, 1.0f);
	}

	if (_mode == EDisplayMode::Bar && _progressBar)
	{
		_progressBar->SetProgressPos(_value);
	}
	else if (_mode == EDisplayMode::Percent && _percentText)
	{
		updatePercentText();
	}
}

void CUIItemStateDisplay::SetPortion(int current, int max)
{
	_portionCurrent = current;
	_portionMax = max;
	clamp(_portionMax, 1, 255);
	clamp(_portionCurrent, 0, _portionMax);
	_hasPortionData = true;

	if (HasPortionBar())
	{
		_value = 0.0f;
		if (_portionCurrent > 0)
		{
			_value = _portionMax <= PortionBarSegments
				? (static_cast<float>(_portionCurrent - 1) + PortionBarCut) / static_cast<float>(PortionBarSegments)
				: static_cast<float>(_portionCurrent) / static_cast<float>(_portionMax);
		}
		// Discrete charges must not animate through fractions of the next dash.
		PortionBar->SetProgressPosImmediate(_value);
	}

	if (_mode == EDisplayMode::Percent && _percentText)
	{
		updatePercentText();
	}
}

void CUIItemStateDisplay::ShowBackground(bool status)
{
	_showBackground = status;
	if (_mode == EDisplayMode::Bar && _progressBar)
	{
		_progressBar->ShowBackground(status);
	}
}

void CUIItemStateDisplay::updatePercentText()
{
	if (!_percentText)
	{
		return;
	}

	string32 buf;
	int displayVal = 0;
	switch (_percentFormat)
	{
		case EPercentFormat::Percent:
		{
			displayVal = (int)(_value * _fractionMax + 0.5f);
			clamp(displayVal, 0, _fractionMax);
			xr_sprintf(buf, "%d%%", displayVal);
			break;
		}
		case EPercentFormat::Number:
		{
			displayVal = (int)(_value * _fractionMax + 0.5f);
			clamp(displayVal, 0, _fractionMax);
			xr_sprintf(buf, "%d", displayVal);
			break;
		}
		case EPercentFormat::Fraction:
		{
			displayVal = (int)(_value * _fractionMax + 0.5f);
			clamp(displayVal, 0, _fractionMax);
			xr_sprintf(buf, "%d/%d", displayVal, _fractionMax);
			break;
		}
		case EPercentFormat::Portion:
		{
			if (!_hasPortionData || _portionMax <= 1)
			{
				displayVal = (int)(_value * _fractionMax + 0.5f);
				clamp(displayVal, 0, _fractionMax);
				xr_sprintf(buf, "%d%%", displayVal);
			}
			else
			{
				xr_sprintf(buf, "%d/%d", _portionCurrent, _portionMax);
			}
			break;
		}
		default:
		{
			displayVal = (int)(_value * _fractionMax + 0.5f);
			clamp(displayVal, 0, _fractionMax);
			xr_sprintf(buf, "%d%%", displayVal);
			break;
		}
	}
	_percentText->SetText(buf);

	if (_useTextColor)
	{
		Fcolor currentColor;
		if (_useTextGradient)
		{
			if (_useMiddleTextColor)
			{
				currentColor.lerp(_minTextColor, _middleTextColor, _maxTextColor, _value);
			}
			else
			{
				currentColor.lerp(_minTextColor, _maxTextColor, _value);
			}
		}
		else
		{
			currentColor = _maxTextColor;
		}

		CUILines* lines = _percentText->TextItemControl();
		if (lines)
		{
			lines->SetTextColor(currentColor.get());
		}
	}
}

void CUIItemStateDisplay::Draw()
{
	Frect rect;
	GetAbsoluteRect(rect);

	if (_mode == EDisplayMode::Bar && _progressBar)
	{
		CUIProgressBar* ActiveBar = _hasPortionData && PortionBar ? PortionBar : _progressBar;
		ActiveBar->ShowBackground(_showBackground);
		ActiveBar->m_bUseGradient = m_bUseGradient;
		UI().PushScissor(rect);
		ActiveBar->Draw();
		UI().PopScissor();
		return;
	}

	if (_mode == EDisplayMode::Percent)
	{
		if (_percentBackground)
		{
			UI().PushScissor(rect);
			_percentBackground->Draw();
			UI().PopScissor();
		}
		return;
	}
}

void CUIItemStateDisplay::Update()
{
	inherited::Update();
	if (_mode == EDisplayMode::Bar && _progressBar)
	{
		_progressBar->Update();
	}
}
