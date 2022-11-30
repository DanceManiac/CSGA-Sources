// File:		UIDebugFonts.cpp
// Description:	Output list of all fonts
// Created:		22.03.2005
// Author:		Serge Vynnychenko
// Mail:		narrator@gsc-game.kiev.ua
//
// Copyright 2005 GSC Game World

#include "StdAfx.h"
#include "UIDebugFonts.h"
#include "dinput.h"
#include "../hudmanager.h"


CUIDebugFonts::CUIDebugFonts()
{
	AttachChild			(&m_background);
	InitDebugFonts		(Frect().set(0, 0, UI_BASE_WIDTH, UI_BASE_HEIGHT));
}

CUIDebugFonts::~CUIDebugFonts(){

}

void CUIDebugFonts::InitDebugFonts(Frect r)
{
	CUIDialogWnd::SetWndRect	(r);

	FillUpList();

	m_background.SetWndRect	(r);
	m_background.InitTexture	("ui\\ui_debug_font");
}

bool CUIDebugFonts::OnKeyboard(int dik, EUIMessages keyboard_action){
	if (DIK_ESCAPE == dik)
		this->GetHolder()->StartStopMenu(this, true);

	if (DIK_F12 == dik)
		return false;

    return true;
}
#include "../string_table.h"

void CUIDebugFonts::FillUpList()
{
	Fvector2 pos, sz;
	pos.set(0.0f, 0.0f);
	sz.set(UI_BASE_WIDTH, UI_BASE_HEIGHT);
	string256 str;
	for (auto ppFont : UI().Font().FontVect)
	{
		if (!ppFont.first)
			continue;

		xr_sprintf(str, "%s:%s", (ppFont.first)->m_font_name.c_str(), CStringTable().translate("Test_Font_String").c_str());

		CUIStatic* pItem = xr_new<CUIStatic>();
		pItem->SetWndPos			(pos);
		pItem->SetWndSize			(sz);
		pItem->SetFont				(ppFont.first);
		pItem->SetText				(str);
		pItem->SetTextComplexMode	(false);
		pItem->SetVTextAlignment	(valCenter);
		pItem->SetTextAlignment		(CGameFont::alCenter);
		pItem->AdjustHeightToText	();
		pItem->SetAutoDelete		(true);

		pos.y += pItem->GetHeight() + 20.0f;

		AttachChild(pItem);
	}
}