#include "stdafx.h"
#include "HudItem.h"
#include "physic_item.h"
#include "actor.h"
#include "actoreffector.h"
#include "Missile.h"
#include "xrmessages.h"
#include "level.h"
#include "inventory.h"
#include "../xrEngine/CameraBase.h"
#include "player_hud.h"
#include "../xrEngine/SkeletonMotions.h"
#include "ui_base.h"

CHudItem::CHudItem()
{
	RenderHud					(TRUE);
	EnableHudInertion			(TRUE);
	AllowHudInertion			(TRUE);
	m_bStopAtEndAnimIsRunning	= false;
	m_current_motion_def		= NULL;
	m_started_rnd_anim_idx		= u8(-1);
	SprintType					= false;
}

DLL_Pure *CHudItem::_construct()
{
	m_object			= smart_cast<CPhysicItem*>(this);
	VERIFY				(m_object);

	m_item				= smart_cast<CInventoryItem*>(this);
	VERIFY				(m_item);

	return				(m_object);
}

CHudItem::~CHudItem()
{}

void CHudItem::Load(LPCSTR section)
{
	hud_sect = pSettings->r_string(section,"hud");
	m_animation_slot = pSettings->r_u32(section,"animation_slot");

	m_bDisableBore = !!READ_IF_EXISTS(pSettings, r_bool, hud_sect, "disable_bore", false);

	m_sounds.LoadSound(section, "snd_sprint_start", "sndSprintStart", true);
	m_sounds.LoadSound(section, "snd_sprint_end", "sndSprintEnd", true);

	if(!m_bDisableBore)
		m_sounds.LoadSound(section,"snd_bore","sndBore", true);
}

void CHudItem::PlaySound(LPCSTR alias, const Fvector& position)
{
	m_sounds.PlaySound(alias, position, object().H_Root(), !!GetHUDmode());
}

void CHudItem::renderable_Render()
{
	UpdateXForm					();
	BOOL _hud_render			= ::Render->get_HUD() && GetHUDmode();
	
	if(_hud_render  && !IsHidden())
	{ 
	}
	else 
	{
		if (!object().H_Parent() || (!_hud_render && !IsHidden()))
		{
			on_renderable_Render		();
			debug_draw_firedeps			();
		}else
		if (object().H_Parent()) 
		{
			CInventoryOwner	*owner = smart_cast<CInventoryOwner*>(object().H_Parent());
			VERIFY			(owner);
			CInventoryItem	*self = smart_cast<CInventoryItem*>(this);
			if (owner->attached(self))
				on_renderable_Render();
		}
	}
}

void CHudItem::SwitchState(u32 S)
{
	if (OnClient()) 
		return;

	SetNextState( S );

	if (object().Local() && !object().getDestroy())	
	{
		// !!! Just single entry for given state !!!
		NET_Packet				P;
		object().u_EventGen		(P,GE_WPN_STATE_CHANGE,object().ID());
		P.w_u8					(u8(S));
		object().u_EventSend	(P);
	}
}

void CHudItem::OnEvent(NET_Packet& P, u16 type)
{
	switch (type)
	{
	case GE_WPN_STATE_CHANGE:
		{
			u8				S;
			P.r_u8			(S);
			OnStateSwitch	(u32(S));
		}
		break;
	}
}

void CHudItem::OnStateSwitch(u32 S)
{
	SetState			(S);
	
	if(object().Remote()) 
		SetNextState	(S);

	switch (S)
	{
		case eBore:
		{
			SetPending(true);
			PlayAnimBore();
			if(HudItemData() && !m_bDisableBore)
			{
				Fvector P = HudItemData()->m_item_transform.c;
				m_sounds.PlaySound("sndBore", P, object().H_Root(), !!GetHUDmode(), false, m_started_rnd_anim_idx);
			}
		}break;
		case eSprintStart:
		{
			SetPending(true);
			PlayAnimIdleSprintStart();
            PlaySound("sndSprintStart", object().H_Parent()->Position());
			SprintType = true;
		}break;
		case eSprintEnd:
		{
			SetPending(true);
			PlayAnimIdleSprintEnd();
			PlaySound("sndSprintEnd", object().H_Parent()->Position());
			SprintType = false;
		}break;
	}

    if (S != eIdle && S != eSprintStart && S != eSprintEnd)
        SprintType = false;
}

void CHudItem::OnAnimationEnd(u32 state)
{
	switch(state)
	{
		case eBore:
			SwitchState(eIdle);
		break;
		case eSprintStart:
			SwitchState(eIdle);
		break;
		case eSprintEnd:
			SwitchState(eIdle);
		break;
	}
}

void CHudItem::PlayAnimBore()
{
	PlayHUDMotion ("anm_bore", TRUE, this, GetState());
}

bool CHudItem::ActivateItem() 
{
	OnActiveItem	();
	return			true;
}

void CHudItem::DeactivateItem() 
{
	OnHiddenItem	();
}
void CHudItem::OnMoveToRuck(EItemPlace prev)
{
	SwitchState(eHidden);
}

void CHudItem::SendDeactivateItem	()
{
	SendHiddenItem	();
}

void CHudItem::SendHiddenItem()
{
	if (!object().getDestroy())
	{
		NET_Packet				P;
		object().u_EventGen		(P,GE_WPN_STATE_CHANGE,object().ID());
		P.w_u8					(u8(eHiding));
		object().u_EventSend	(P, net_flags(TRUE, TRUE, FALSE, TRUE));
	}
}

void CHudItem::UpdateHudAdditonal		(Fmatrix& hud_trans)
{}

void CHudItem::UpdateCL()
{
	if(m_current_motion_def)
	{
		if(m_bStopAtEndAnimIsRunning)
		{
			const xr_vector<motion_marks>&	marks = m_current_motion_def->marks;
			if(!marks.empty())
			{
				float motion_prev_time = ((float)m_dwMotionCurrTm - (float)m_dwMotionStartTm)/1000.0f;
				float motion_curr_time = ((float)Device.dwTimeGlobal - (float)m_dwMotionStartTm)/1000.0f;
				
				xr_vector<motion_marks>::const_iterator it = marks.begin();
				xr_vector<motion_marks>::const_iterator it_e = marks.end();
				for(;it!=it_e;++it)
				{
					const motion_marks&	M = (*it);
					if(M.is_empty())
						continue;
	
					const motion_marks::interval* Iprev = M.pick_mark(motion_prev_time);
					const motion_marks::interval* Icurr = M.pick_mark(motion_curr_time);
					if(Iprev==NULL && Icurr!=NULL)
					{
						OnMotionMark				(m_startedMotionState, M);
					}
				}
			
			}

			m_dwMotionCurrTm					= Device.dwTimeGlobal;
			if(m_dwMotionCurrTm > m_dwMotionEndTm)
			{
				m_current_motion_def				= NULL;
				m_dwMotionStartTm					= 0;
				m_dwMotionEndTm						= 0;
				m_dwMotionCurrTm					= 0;
				m_bStopAtEndAnimIsRunning			= false;
				OnAnimationEnd						(m_startedMotionState);
			}
		}
	}
}

void CHudItem::OnH_A_Chield		()
{}

void CHudItem::OnH_B_Chield		()
{
	StopCurrentAnimWithoutCallback();
}

void CHudItem::OnH_B_Independent	(bool just_before_destroy)
{
	m_sounds.StopAllSounds	();
	UpdateXForm				();
}

void CHudItem::OnH_A_Independent	()
{
	if(HudItemData())
		g_player_hud->detach_item(this);
	StopCurrentAnimWithoutCallback();
}

void CHudItem::on_b_hud_detach()
{
	m_sounds.StopAllSounds	();
}

void CHudItem::on_a_hud_attach()
{
	if(m_current_motion_def)
	{
		PlayHUDMotion_noCB(m_current_motion, FALSE);
#ifdef DEBUG
		Msg("continue playing [%s][%d]",m_current_motion.c_str(), Device.dwFrame);
#endif // #ifdef DEBUG
	}
}

u32 CHudItem::PlayHUDMotion(const shared_str& M, BOOL bMixIn, CHudItem*  W, u32 state)
{
	u32 anim_time = PlayHUDMotion_noCB(M, bMixIn);
	if (anim_time > 0)
	{
		m_bStopAtEndAnimIsRunning = true;
		m_dwMotionStartTm = Device.dwTimeGlobal;
		m_dwMotionCurrTm = m_dwMotionStartTm;
		m_dwMotionEndTm = m_dwMotionStartTm + anim_time;
		m_startedMotionState = state;
	}
	else
		m_bStopAtEndAnimIsRunning = false;

	return anim_time;
}

//AVO: check if animation exists
bool CHudItem::isHUDAnimationExist(LPCSTR anim_name)
{
	if (HudItemData()) // First person
	{
		string256 anim_name_r;
		bool is_16x9 = ui_core().is_widescreen();
		u16 attach_place_idx = pSettings->r_u16(HudItemData()->m_sect_name, "attach_place_idx");
		sprintf(anim_name_r, "%s%s", anim_name, (attach_place_idx == 1 && is_16x9) ? "_16x9" : "");
		player_hud_motion* anm = HudItemData()->m_hand_motions.find_motion(anim_name_r);
		if (anm)
			return true;
	}
	else {// Third person
		if (g_player_hud->motion_length(anim_name, HudSection(), m_current_motion_def) > 100)
			return true;
	}


#ifdef DEBUG
	Msg("~ [WARNING] ------ Animation [%s] does not exist in [%s]", anim_name, HudSection().c_str());
#endif
	return false;
}

u32 CHudItem::PlayHUDMotion_noCB(const shared_str& motion_name, const bool bMixIn, const bool randomAnim)
{
	m_current_motion					= motion_name;

	if(bDebug && item().m_pInventory)
	{
		Msg("-[%s] as[%d] [%d]anim_play [%s][%d]",
			HudItemData()?"HUD":"Simulating", 
			item().m_pInventory->GetActiveSlot(), 
			item().object_id(),
			motion_name.c_str(), 
			Device.dwFrame);
	}
	if( HudItemData() )
	{
		return HudItemData()->anim_play		(motion_name, bMixIn, m_current_motion_def, m_started_rnd_anim_idx);
	}else
	{
		m_started_rnd_anim_idx				= 0;
		return g_player_hud->motion_length	(motion_name, HudSection(), m_current_motion_def );
	}
}

void CHudItem::StopCurrentAnimWithoutCallback()
{
	m_dwMotionStartTm			= 0;
	m_dwMotionEndTm				= 0;
	m_dwMotionCurrTm			= 0;
	m_bStopAtEndAnimIsRunning	= false;
	m_current_motion_def		= NULL;
}

BOOL CHudItem::GetHUDmode()
{
	if(object().H_Parent())
	{
		CActor* A = smart_cast<CActor*>(object().H_Parent());
		return ( A && A->HUDview() && HudItemData() && 
				(HudItemData())
			);
	}else
		return FALSE;
}

void CHudItem::PlayAnimIdle()
{
	if (TryPlayAnimIdle())
		return;

	PlayHUDMotion("anm_idle", true, nullptr, GetState());
}

bool CHudItem::TryPlayAnimIdle()
{
	if(MovingAnimAllowedNow())
	{
        auto pActor = smart_cast<CActor*>(object().H_Parent());
		if(pActor)
		{
            u32 state = pActor->get_state();
            if (state & mcSprint)
            {
				if(!SprintType)
					SwitchState(eSprintStart);
				if(GetState() != eSprintStart || GetState() != eSprintEnd)
					PlayAnimIdleSprint();
                return true;
            }
            else if (SprintType)
			{
				SwitchState(eSprintEnd);
				return true;
			}
            else if ((state & mcAnyMove))
            {
                if (!(state & mcCrouch))
                {
                    if (state & mcAccel)
                        PlayAnimIdleMovingSlow();
                    else
                        PlayAnimIdleMoving();
                    return true;
                }
                else if (state & mcAccel)
                {
                    PlayAnimIdleMovingCrouchSlow();
                    return true;
                }
                else
                {
                    PlayAnimIdleMovingCrouch();
                    return true;
				}
			}
		}
	}
	return false;
}

void CHudItem::PlayAnimIdleMoving()
{
	PlayHUDMotion("anm_idle_moving", true, nullptr, GetState());
}

void CHudItem::PlayAnimIdleSprintStart()
{
	PlayHUDMotion("anm_idle_sprint_start", true, nullptr, GetState());
}

void CHudItem::PlayAnimIdleSprint()
{
	PlayHUDMotion("anm_idle_sprint", true, nullptr, GetState());
}

void CHudItem::PlayAnimIdleSprintEnd()
{
	PlayHUDMotion("anm_idle_sprint_end", true, nullptr, GetState());
}

void CHudItem::PlayAnimIdleMovingCrouch()
{
	PlayHUDMotion("anm_idle_moving_crouch", true, nullptr, GetState());
}

void CHudItem::PlayAnimIdleMovingCrouchSlow()
{
	PlayHUDMotion("anm_idle_moving_crouch_slow", true, nullptr, GetState());
}

void CHudItem::PlayAnimIdleMovingSlow()
{
	PlayHUDMotion("anm_idle_moving_slow", true, nullptr, GetState());
}

void CHudItem::OnMovementChanged(ACTOR_DEFS::EMoveCommand cmd)
{
    if (GetState() == eIdle && !m_bStopAtEndAnimIsRunning)
    {
        PlayAnimIdle();
        ResetSubStateTime();
    }
}

attachable_hud_item* CHudItem::HudItemData()
{
	attachable_hud_item* hi = NULL;
	if(!g_player_hud)		
		return				hi;

	hi = g_player_hud->attached_item(0);
	if (hi && hi->m_parent_hud_item == this)
		return hi;

	hi = g_player_hud->attached_item(1);
	if (hi && hi->m_parent_hud_item == this)
		return hi;

	return NULL;
}
