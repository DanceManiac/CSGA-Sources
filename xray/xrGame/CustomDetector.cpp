#include "stdafx.h"
#include "CustomDetector.h"
#include "Actor.h"
#include "Inventory.h"
#include "Level.h"
#include "Weapon.h"
#include "player_hud.h"
#include "ui/ArtefactDetectorUI.h"
#include "Missile.h"

bool  CCustomDetector::CheckCompatibilityInt(CHudItem* itm)
{
	if(itm==NULL)
		return true;

	CInventoryItem iitm				= itm->item();
	u32 slot						= iitm.GetSlot();
	bool bres = (slot==PISTOL_SLOT || slot==KNIFE_SLOT || slot==BOLT_SLOT);

	if(itm->GetState()!=CHUDState::eShowing)
		bres = bres && !itm->IsPending();

	if(bres)
	{
		CWeapon* W = smart_cast<CWeapon*>(itm);
		if (W)
			bres = bres
				&& (W->GetState() != CHUDState::eBore)
				&& (W->GetState() != CWeapon::eReload)
				&& (W->GetState() != CWeapon::eSwitch);
	}
	if (bres)
		m_lastParentSlot = slot;
	return bres;
}

bool  CCustomDetector::CheckCompatibility(CHudItem* itm)
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
	if(GetState()==eIdle)
		ToggleDetector(bFastMode);
}

void CCustomDetector::ShowDetector(bool bFastMode)
{
	if(GetState()==eHidden)
		ToggleDetector(bFastMode);
}

void CCustomDetector::ToggleDetector(bool bFastMode)
{
	m_bNeedActivation = false;
	m_bFastAnimMode = bFastMode;
	if(GetState()==eHidden)
	{
		PIItem iitem = m_pInventory->ActiveItem();
		CHudItem* itm = (iitem)?iitem->cast_hud_item():NULL;
		if(CheckCompatibilityInt(itm))
		{
			SwitchState				(eShowing);
			TurnDetectorInternal	(true);
		}
		else
		{
			m_pInventory->Activate(m_lastParentSlot);
			m_bNeedActivation = true;
		}
	}
	else
	if(GetState()==eIdle)
		SwitchState					(eHiding);
}

void CCustomDetector::OnStateSwitch(u32 S)
{
	u32 oldState = GetState();
	inherited::OnStateSwitch(S);

	switch(S)
	{
	case eShowing:
		{
			g_player_hud->attach_item	(this);
			m_sounds.PlaySound			("sndShow", Fvector().set(0,0,0), this, true, false);
			PlayHUDMotion				(m_bFastAnimMode?"anm_show_fast":"anm_show", TRUE, this, GetState());
			SetPending					(TRUE);
		}break;
	case eHiding:
		{
			if(oldState != eHiding)
			{
				m_sounds.PlaySound			("sndHide", Fvector().set(0,0,0), this, true, false);
				PlayHUDMotion				(m_bFastAnimMode?"anm_hide_fast":"anm_hide", TRUE, this, GetState());
				SetPending					(TRUE);
			}
		}break;
	case eIdle:
		{
			PlayAnimIdle				();
			SetPending					(FALSE);
		}break;
	case eIdleThrowStart:
		{
			PlayHUDMotion ("anm_throw_begin", TRUE, this, GetState());
			SetPending(TRUE);
		}break;
	case eIdleThrow:
		{
			PlayHUDMotion ("anm_throw_idle", TRUE, this, GetState());
			SetPending(TRUE);
		}break;
	case eIdleThrowEnd:
		{
			if(isHUDAnimationExist("anm_throw"))
			{
				PlayHUDMotion ("anm_throw", TRUE, this, GetState());
				SetPending(TRUE);
			}
			else
				SwitchState(eIdle);
		}break;
	case eIdleKick:
		{
			PlayHUDMotion ("anm_kick", TRUE, this, GetState());
			SetPending(TRUE);
		}break;
	case eIdleKick2:
		{
			PlayHUDMotion ("anm_kick2", TRUE, this, GetState());
			SetPending(TRUE);
		}break;
	case eIdleZoom:
		{
			PlayHUDMotion ("anm_idle_zoom", TRUE, this, GetState());
			SetPending(FALSE);
		}break;
	case eIdleZoomIn:
		{
			PlayHUDMotion ("anm_zoom_in", TRUE, this, GetState());
			SetPending(FALSE);
		}break;
	case eIdleZoomOut:
		{
			PlayHUDMotion ("anm_zoom_out", TRUE, this, GetState());
			SetPending(FALSE);
		}break;
	}
}

void CCustomDetector::OnAnimationEnd(u32 state)
{
	inherited::OnAnimationEnd	(state);
	switch(state)
	{
	case eShowing:
		{
			SwitchState					(eIdle);
		} break;
	case eHiding:
		{
			SwitchState					(eHidden);
			TurnDetectorInternal		(false);
			g_player_hud->detach_item	(this);
		} break;
	case eIdleThrowStart:
		{
			SwitchState(eIdleThrow);
		}break;
	case eIdleThrowEnd:
		{
			SwitchState(eIdle);
		}break;
	case eIdleKick:
		{
			SwitchState(eIdle);
		}break;
	case eIdleKick2:
		{
			SwitchState(eIdle);
		}break;
	case eIdleZoomIn:
		{
			SwitchState(eIdleZoom);
		}break;
	case eIdleZoomOut:
		{
			SwitchState(eIdle);
		}break;
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
{
}

CCustomDetector::CCustomDetector() 
{
	m_ui				= NULL;
	m_bFastAnimMode		= false;
	m_bNeedActivation	= false;
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

	m_sounds.LoadSound( section, "snd_draw", "sndShow", false);
	m_sounds.LoadSound( section, "snd_holster", "sndHide", false);
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
	UpdateAf				();
	m_ui->update			();
}

void CCustomDetector::UpdateVisibility()
{


	//check visibility
	attachable_hud_item* i0		= g_player_hud->attached_item(0);
	if(i0 && HudItemData())
	{
		bool bClimb			= ( (Actor()->MovingState()&mcClimb) != 0 );
		if(bClimb)
		{
			HideDetector		(true);
			m_bNeedActivation	= true;
		}
		else
		{
			CWeapon* wpn = smart_cast<CWeapon*>(i0->m_parent_hud_item);
			CMissile* msl = smart_cast<CMissile*>(i0->m_parent_hud_item);
			if(msl)
			{
				u32 state = msl->GetState();
				if ((state == CMissile::eThrowStart || state == CMissile::eReady) && GetState() == eIdle)
					SwitchState(eIdleThrowStart);
				else if (state == CMissile::eThrow && GetState() == eIdleThrow)
					SwitchState(eIdleThrowEnd);
				else if (state == CMissile::eHiding && (GetState() == eIdleThrowStart || GetState() == eIdleThrow))
					SwitchState(eIdle);
			}
			if(wpn)
			{
				u32 state = wpn->GetState();
			
				if(state==CWeapon::eReload || state==CWeapon::eSwitch)
				{
					HideDetector		(true);
					m_bNeedActivation	= true;
				}
				else if (wpn->IsZoomed())
				{
					if(GetState() == eIdle || GetState() == eIdleZoomOut)
						SwitchState(eIdleZoomIn);
				}
				else if (GetState() == eIdleZoom || GetState() == eIdleZoomIn)
					SwitchState(eIdleZoomOut);
			}
		}
	}
	else
	if(m_bNeedActivation)
	{
		attachable_hud_item* i0		= g_player_hud->attached_item(0);
		bool bClimb					= ( (Actor()->MovingState()&mcClimb) != 0 );
		if(!bClimb)
		{
			CWeapon* wpn			= (i0)?smart_cast<CWeapon*>(i0->m_parent_hud_item) : NULL;
			if(	!wpn || (wpn->GetState()!=CWeapon::eReload && wpn->GetState()!=CWeapon::eSwitch))
			{
				ShowDetector		(true);
			}
		}
	}
}

void CCustomDetector::UpdateCL() 
{
	inherited::UpdateCL();

	UpdateVisibility		();

	if( !IsWorking() )		return;
	UpfateWork				();
}

void CCustomDetector::OnH_A_Chield() 
{
	inherited::OnH_A_Chield		();
}

void CCustomDetector::OnH_B_Independent(bool just_before_destroy) 
{
	inherited::OnH_B_Independent(just_before_destroy);
	
	m_artefacts.clear			();
}


void CCustomDetector::OnMoveToRuck(EItemPlace prev)
{
	inherited::OnMoveToRuck	(prev);
	if(GetState()==eIdle)
	{
		SwitchState					(eHidden);
		g_player_hud->detach_item	(this);
	}
	TurnDetectorInternal	(false);
}

void CCustomDetector::OnMoveToSlot()
{
	inherited::OnMoveToSlot	();
}

void CCustomDetector::TurnDetectorInternal(bool b)
{
	m_bWorking				= b;
	if(b && m_ui==NULL)
	{
		CreateUI			();
	}else
	{
//.		xr_delete			(m_ui);
	}

	UpdateNightVisionMode	(b);
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
		CArtefact*	pAf				= smart_cast<CArtefact*>(O);
		
		if(pAf->GetAfRank()>m_af_rank)
			res = false;
	}
	return						res;
}
