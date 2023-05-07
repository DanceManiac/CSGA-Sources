#include "stdafx.h"
#include "CustomDetector.h"
#include "Actor.h"
#include "Inventory.h"
#include "Level.h"
#include "Weapon.h"
#include "player_hud.h"
#include "ui/ArtefactDetectorUI.h"
#include "WeaponKnife.h"

bool CCustomDetector::CheckCompatibilityInt(CHudItem* itm)
{
	if(itm==nullptr)
		return true;

	CInventoryItem iitm				= itm->item();
	u32 slot						= iitm.GetSlot();
	bool bres = (slot==PISTOL_SLOT || slot==KNIFE_SLOT || slot==BOLT_SLOT);

	CWeaponKnife* WK = dynamic_cast<CWeaponKnife*>(itm);
	if(!WK && itm->GetState()!=CHUDState::eShowing)
		bres = bres && !itm->IsPending();

	if(bres)
	{
		CWeapon* W = dynamic_cast<CWeapon*>(itm);
		if (W)
			bres = bres
				&& (W->GetState() != CHUDState::eBore)
				&& (W->GetState() != CWeapon::eReload)
				&& (W->GetState() != CWeapon::eSwitch)
				&& !W->IsZoomed();
	}

	return bres;
}

bool CCustomDetector::CheckCompatibility(CHudItem* itm)
{
	if(!inherited::CheckCompatibility(itm) )	
		return false;

	if(!CheckCompatibilityInt(itm))
	{
		HideDetector	(true);
		return			false;
	}
	return true;
}

void CCustomDetector::HideDetector(bool bFastMode)
{
    if (GetState() == eIdle)
		ToggleDetector(bFastMode);
}

void CCustomDetector::ShowDetector(bool bFastMode)
{
    if (GetState() == eHidden)
		ToggleDetector(bFastMode);
}

void CCustomDetector::ToggleDetector(bool bFastMode)
{
    m_bFastAnimMode = bFastMode;

    CWeapon* wpn = dynamic_cast<CWeapon*>(m_pInventory->ActiveItem());
    CWeaponKnife* knf = dynamic_cast<CWeaponKnife*>(m_pInventory->ActiveItem());

    if (GetState() == eHidden)
	{
        PIItem iitem = m_pInventory->ActiveItem();
        CHudItem* itm = (iitem) ? iitem->cast_hud_item() : NULL;

		if (CheckCompatibilityInt(itm))
		{
			if (wpn)
			{
                if (knf || wpn->bIsNeedCallDet)
				{
					SwitchState(eShowing);
					TurnDetectorInternal(true);
					wpn->bIsNeedCallDet = false;
				}
				else
				{
					if(wpn->GetState() == CWeapon::eIdle || wpn->GetState() == CWeapon::eEmpty)
						wpn->SwitchState(CWeapon::eShowingDet);
				}
			}
			else
			{
				SwitchState(eShowing);
				TurnDetectorInternal(true);
			}
        }
    }
	else if (GetState() == eIdle)
        SwitchState(eHiding);

	m_bNeedActivation = false;
}

void CCustomDetector::OnStateSwitch(u32 S)
{
	u32 oldState = GetState();
	inherited::OnStateSwitch(S);

	switch(S)
	{
	case eShowing:
		{
			g_player_hud->attach_item(this);
			m_sounds.PlaySound("sndShow", Fvector().set(0,0,0), this, true, false);
			PlayHUDMotion(m_bFastAnimMode?"anm_show_fast":"anm_show", false, this, GetState());
			SetPending(true);
		}break;
	case eHiding:
		{
			if(oldState != eHiding)
			{
				m_sounds.PlaySound("sndHide", Fvector().set(0,0,0), this, true, false);
				PlayHUDMotion(m_bFastAnimMode?"anm_hide_fast":"anm_hide", true, this, GetState());
                SetPending(true);
				SetHideDetStateInWeapon();
			}
		}break;
	case eIdle:
		{
			PlayAnimIdle();
			SetPending(false);
		}break;
	case eShowingParItm:
		{
			PlayHUDMotion("anm_show_right_hand", TRUE, this, eShowingParItm);
			SetPending(TRUE);
		}break;
	case eHideParItm:
		{
			PlayHUDMotion("anm_hide_right_hand", TRUE, this, eHideParItm);
			SetPending(TRUE);
		}break;
	case eFireDet:
		{
			if(bZoomed)
				PlayHUDMotion("anm_aim_wpn_shoot_det", TRUE, this, eFireDet);
			else
				PlayHUDMotion("anm_wpn_shoot_det", TRUE, this, eFireDet);

			SetPending(FALSE);
		}break;
	case eEmptyDet:
		{
			if(bZoomed)
				PlayHUDMotion("anm_aim_wpn_shoot_empty_det", TRUE, this, eEmptyDet);
			else
				PlayHUDMotion("anm_wpn_shoot_empty_det", TRUE, this, eEmptyDet);

			SetPending(FALSE);
		}break;
        case eKickKnf:
		{
			PlayHUDMotion("anm_knf_kick_det", TRUE, this, eKickKnf);
			SetPending(FALSE);
		}break;
        case eKickKnf2:
		{
			PlayHUDMotion("anm_knf_kick2_det", TRUE, this, eKickKnf);
			SetPending(FALSE);
		}break;
	case eUnLightMisDet:
		{
			if(bZoomed)
				PlayHUDMotion("anm_aim_wpn_lightmis_det", TRUE, this, eUnLightMisDet);
			else
				PlayHUDMotion("anm_wpn_lightmis_det", TRUE, this, eUnLightMisDet);
			SetPending(TRUE);
		}break;
	case eLookMisDet:
		{
			if(bZoomed)
				PlayHUDMotion("anm_aim_wpn_lookmis_det", TRUE, this, eLookMisDet);
			else
				PlayHUDMotion("anm_wpn_lookmis_det", TRUE, this, eLookMisDet);
			SetPending(TRUE);
		}break;
	case eSwitchModeDet:
		{
			PlayHUDMotion("anm_wpn_firemode_det", TRUE, this, eSwitchModeDet);
			SetPending(TRUE);
		}break;
        case eThrowStartMis:
		{
			PlayHUDMotion("anm_throw_start_det", TRUE, this, eThrowStartMis);
			SetPending(FALSE);
		}break;
        case eReadyMis:
		{
			PlayHUDMotion("anm_throw_det", TRUE, this, eThrowStartMis);
			SetPending(FALSE);
		}break;
        case eThrowMis:
		{
			PlayHUDMotion("anm_throw_end_det", TRUE, this, eThrowMis);
			SetPending(FALSE);
		}break;
        case eZoomStartDet:
		{
			PlayHUDMotion("anm_zoom_start_det", TRUE, this, eZoomStartDet);
			SetPending(FALSE);
			bZoomed = true;
		}break;
        case eZoomEndDet:
		{
			PlayHUDMotion("anm_zoom_end_det", TRUE, this, eZoomEndDet);
			SetPending(FALSE);
			bZoomed = false;
		}break;
	}
}

bool CCustomDetector::TryPlayAnimIdle()
{
	if (bZoomed)
		return false;

	return inherited::TryPlayAnimIdle();
}

const char* CCustomDetector::GetAnimAimName()
{
	auto pActor = dynamic_cast<const CActor*>(H_Parent());
	if (pActor)
	{
        u32 state = pActor->get_state();
        if (state & mcAnyMove)
		{
			std::string anm_name = "";
			if (state & mcFwd)
				anm_name += "_forward";
			else if (state & mcBack)
				anm_name += "_back";
			else if (state & mcLStrafe)
				anm_name += "_left";
			else if (state & mcRStrafe)
				anm_name += "_right";

			return xr_strconcat(guns_aim_det_anm, "anm_idle_aim_moving_det", anm_name.c_str());
		}
	}
	return nullptr;
}

void CCustomDetector::PlayAnimIdle()
{
	if (TryPlayAnimIdle())
		return;

	if (!bZoomed)
	{
		bZoomed = false;
		PlayHUDMotion("anm_idle", true, nullptr, GetState());
	}
	else
	{
		bZoomed = true;
		if (const char* guns_aim_det_anm = GetAnimAimName())
			PlayHUDMotion(guns_aim_det_anm, true, this, GetState());
		else
			PlayHUDMotion("anm_idle_aim", true, nullptr, GetState());
	}
}

void CCustomDetector::SetHideDetStateInWeapon()
{
	CWeapon* wpn = dynamic_cast<CWeapon*>(m_pInventory->ActiveItem());

	if (!wpn)
		return;

	CWeaponKnife* knf = dynamic_cast<CWeaponKnife*>(m_pInventory->ActiveItem());

	if (knf)
		return;

	if (wpn->GetState() == eIdle || wpn->GetState() == CWeapon::eEmpty)
		wpn->SwitchState(CWeapon::eHideDet);
}

void CCustomDetector::OnAnimationEnd(u32 state)
{
	inherited::OnAnimationEnd	(state);
	switch(state)
	{
		case eHiding:
		{
			SwitchState(eHidden);
			TurnDetectorInternal(false);
			g_player_hud->detach_item(this);
		}break;
		case eShowing:
		case eFireDet:
		case eKickKnf:
		case eKickKnf2:
		case eEmptyDet:
		case eShowingParItm:
		case eHideParItm:
		case eZoomStartDet:
		case eZoomEndDet:
		case eLookMisDet:
		case eUnLightMisDet:
		case eSwitchModeDet:
		case eThrowMis:
			SwitchState(eIdle);
		break;
	}
}

void CCustomDetector::UpdateXForm()
{
	CInventoryItem::UpdateXForm();
}

void CCustomDetector::OnActiveItem()
{
	return;
}

void CCustomDetector::OnHiddenItem()
{}

CCustomDetector::CCustomDetector() 
{
	m_ui				= nullptr;
	m_bFastAnimMode		= false;
	m_bNeedActivation	= false;
	bZoomed				= false;
}

CCustomDetector::~CCustomDetector() 
{
	m_artefacts.destroy		();
	TurnDetectorInternal	(false);
	xr_delete				(m_ui);
}

BOOL CCustomDetector::net_Spawn(CSE_Abstract* DC) 
{
	TurnDetectorInternal(false);
	return		(inherited::net_Spawn(DC));
}

void CCustomDetector::Load(LPCSTR section) 
{
	inherited::Load			(section);

	m_fAfDetectRadius		= pSettings->r_float(section,"af_radius");
	m_fAfVisRadius			= pSettings->r_float(section,"af_vis_radius");
	m_artefacts.load		(section, "af");

	m_sounds.LoadSound(section, "snd_draw", "sndShow");
	m_sounds.LoadSound(section, "snd_holster", "sndHide");
}

void CCustomDetector::shedule_Update(u32 dt) 
{
	inherited::shedule_Update(dt);
	
	if( !IsWorking() )			return;

	Position().set(H_Parent()->Position());

	Fvector						P; 
	P.set						(H_Parent()->Position());
	m_artefacts.feel_touch_update(P,m_fAfDetectRadius);
}


bool CCustomDetector::IsWorking()
{
	return m_bWorking && H_Parent() && H_Parent()==Level().CurrentViewEntity();
}

void CCustomDetector::UpfateWork()
{
	UpdateAf();
	m_ui->update();
}

void CCustomDetector::UpdateVisibility()
{
	//check visibility
	attachable_hud_item* i0 = g_player_hud->attached_item(0);
	if(i0 && HudItemData())
	{
		auto wpn = dynamic_cast<CWeapon*>(i0->m_parent_hud_item);
		if(wpn)
		{
			u32 state = wpn->GetState();
			bool bClimb	 = ((Actor()->MovingState()&mcClimb) != 0);
			if(bClimb || state==CWeapon::eReload || state==CWeapon::eSwitch || (state == CWeapon::eSwitchMode && wpn->m_magazine.size() == 0))
			{
				HideDetector(true);
				m_bNeedActivation = true;
			}
		}
	}
	else if(m_bNeedActivation)
	{
		bool bClimb = ((Actor()->MovingState()&mcClimb) != 0);
		if(!bClimb)
		{
			auto wpn = (i0) ? dynamic_cast<CWeapon*>(i0->m_parent_hud_item) : NULL;
			if(!wpn || (!wpn->IsZoomed() && wpn->GetState() != CWeapon::eReload && wpn->GetState() != CWeapon::eSwitch && wpn->GetState() != CWeapon::eSwitchMode))
				ShowDetector(true);
		}
	}
}

void CCustomDetector::UpdateCL() 
{
	inherited::UpdateCL();

	UpdateVisibility();

	if(!IsWorking())
		return;

	UpfateWork();
}

void CCustomDetector::OnH_A_Chield() 
{
	inherited::OnH_A_Chield();
}

void CCustomDetector::OnH_B_Independent(bool just_before_destroy) 
{
	inherited::OnH_B_Independent(just_before_destroy);
	
	m_artefacts.clear();

	if (GetState() != eHidden)
	{
		// Detaching hud item and animation stop in OnH_A_Independent
		TurnDetectorInternal(false);
		SwitchState(eHidden);
	}
}

void CCustomDetector::OnMoveToRuck(EItemPlace prev)
{
	inherited::OnMoveToRuck(prev);
	if(GetState() == eIdle)
	{
		SwitchState(eHidden);
		g_player_hud->detach_item(this);
	}
	TurnDetectorInternal(false);
}

void CCustomDetector::OnMoveToSlot()
{
	inherited::OnMoveToSlot();
}

void CCustomDetector::TurnDetectorInternal(bool b)
{
	m_bWorking = b;
	if(b && m_ui == NULL)
	{
		CreateUI();
	}

	UpdateNightVisionMode(b);
}

#include "game_base_space.h"
void CCustomDetector::UpdateNightVisionMode(bool b_on)
{
}

BOOL CAfList::feel_touch_contact	(CObject* O)
{
	CLASS_ID	clsid			= O->CLS_ID;
	TypesMapIt it				= m_TypesMap.find(clsid);

	bool res					 = (it!=m_TypesMap.end());
	if(res)
	{
		CArtefact*	pAf				= dynamic_cast<CArtefact*>(O);
		
		if(pAf->GetAfRank()>m_af_rank)
			res = false;
	}
	return						res;
}
