#include "stdafx.h"
#include "weaponBM16.h"
#include "Actor.h"

CWeaponBM16::~CWeaponBM16()
{
}

void CWeaponBM16::Load	(LPCSTR section)
{
	inherited::Load		(section);

	m_sounds.LoadSound(section, "snd_changecartridgetype_only", "sndChangeCartridgeFull", true, m_eSoundReload);
	m_sounds.LoadSound(section, "snd_changecartridgetype_one", "sndChangeCartridgeOne", true, m_eSoundReload);
}

void CWeaponBM16::PlayReloadSound()
{
	switch(m_magazine.size())
	{
		case 0:
			PlaySound("sndReloadEmpty", get_LastFP());
		break;
		case 1:
		{
			if(IsMisfire())
				PlaySound("sndReloadJammed", get_LastFP());
			else if (bSwitchAmmoType)
				PlaySound("sndChangeCartridgeOne", get_LastFP());
			else
				PlaySound("sndReload", get_LastFP());
		}break;
		case 2:
		{
			if(IsMisfire())
				PlaySound("sndReloadJammed", get_LastFP());
			else
				PlaySound("sndChangeCartridgeFull", get_LastFP());
		}break;
	}
}

void CWeaponBM16::PlayAnimShoot()
{
	switch(m_magazine.size())
	{
		case 1:
		{
			if (IsZoomed())
			{
				if(IsMisfire())
					PlayHUDMotion("anm_shoot_aim_jammed_1", false, this, GetState());
				else
					PlayHUDMotion("anm_shoot_aim_1", false, this, GetState());
			}
			else
			{
				if(IsMisfire())
					PlayHUDMotion("anm_shoot_jammed_1", false, this, GetState());
				else
					PlayHUDMotion("anm_shoot_1", false, this, GetState());
			}
		}break;
		case 2:
		{
			if (IsZoomed())
			{
				if(IsMisfire())
					PlayHUDMotion("anm_shoot_aim_jammed_2", false, this, GetState());
				else
					PlayHUDMotion("anm_shoot_aim_2", false, this, GetState());
			}
			else
			{
				if(IsMisfire())
					PlayHUDMotion("anm_shoot_jammed_2", false, this, GetState());
				else
					PlayHUDMotion("anm_shoot_2", false, this, GetState());
			}
		}break;
	}
}

void CWeaponBM16::PlayAnimShow()
{
	switch( m_magazine.size() )
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_show_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_show_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_show_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_show_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_show_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_show_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimHide()
{
	switch( m_magazine.size() )
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_hide_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_hide_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_hide_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_hide_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_hide_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_hide_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimReload()
{
	bool b_both = HaveCartridgeInInventory(2);

	VERIFY(GetState()==eReload);

	if(m_magazine.size() == 1 || !b_both)
	{
		if(IsMisfire())
			PlayHUDMotion("anm_reload_jammed_1", true, this, GetState());
		else if (bSwitchAmmoType)
			PlayHUDMotion("anm_reload_ammochange_1", true, this, GetState());
		else
			PlayHUDMotion("anm_reload_1", true, this, GetState());
	}
	else if (m_magazine.size() == 0 && bSwitchAmmoType)
		PlayHUDMotion("anm_reload_0", true, this, GetState());
	else
	{
		if(IsMisfire())
			PlayHUDMotion("anm_reload_jammed_2", true, this, GetState());
		else if (bSwitchAmmoType)
			PlayHUDMotion("anm_reload_ammochange_2", true, this, GetState());
		else
			PlayHUDMotion("anm_reload_2", true, this, GetState());
	}
}

void CWeaponBM16::PlayAnimIdleMoving()
{
	switch( m_magazine.size() )
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimIdleSprintStart()
{
	switch( m_magazine.size() )
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_start_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_start_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_start_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_start_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_start_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_start_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimIdleSprint()
{
	switch( m_magazine.size() )
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimIdleSprintEnd()
{
	switch( m_magazine.size() )
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_end_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_end_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_end_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_sprint_end_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_sprint_end_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimIdleMovingCrouch()
{
	switch(m_magazine.size())
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_crouch_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_crouch_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_crouch_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_crouch_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_crouch_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_crouch_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimIdleMovingCrouchSlow()
{
	switch(m_magazine.size())
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_crouch_slow_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_crouch_slow_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_crouch_slow_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_crouch_slow_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_crouch_slow_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_crouch_slow_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimIdleMovingSlow()
{
	switch(m_magazine.size())
	{
	case 0:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_slow_jammed_0",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_slow_0",TRUE,this,GetState());
		break;
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_slow_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_slow_1",TRUE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_idle_moving_slow_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_idle_moving_slow_2",TRUE,this,GetState());
		break;
	}
}

const char* CWeaponBM16::GetAnimAimName()
{
	auto pActor = dynamic_cast<const CActor*>(H_Parent());
	if (pActor)
	{
        u32 state = pActor->get_state();
        if (state & mcAnyMove)
		{
			if (IsScopeAttached())
			{
                strcpy_s(guns_aim_anm_bm, "anm_idle_aim_scope_moving");
				return guns_aim_anm_bm;
			}
			else
				return xr_strconcat(guns_aim_anm_bm, "anm_idle_aim_moving", (state & mcFwd) ? "_forward" : ((state & mcBack) ? "_back" : ""), (state & mcLStrafe) ? "_left" : ((state & mcRStrafe) ? "_right" : ""), IsMisfire() ? "_jammed" : "", m_magazine.size() == 2 ? "_2" : m_magazine.size() == 1 ? "_1" : m_magazine.size() == 0 ? "_0" : "");
		}
	}
	return nullptr;
}

void CWeaponBM16::PlayAnimIdle()
{
	if(TryPlayAnimIdle())
		return;

	if(IsZoomed())
	{

		if (const char* guns_aim_anm_bm = GetAnimAimName())
			PlayHUDMotion(guns_aim_anm_bm, true, this, GetState());
		else
		{
			switch (m_magazine.size())
			{
				case 0:
				{
					if(IsMisfire())
						PlayHUDMotion("anm_idle_aim_jammed_0", TRUE, NULL, GetState());
					else
						PlayHUDMotion("anm_idle_aim_0", TRUE, NULL, GetState());
				}break;
				case 1:
				{
					if(IsMisfire())
						PlayHUDMotion("anm_idle_aim_jammed_1", TRUE, NULL, GetState());
					else
						PlayHUDMotion("anm_idle_aim_1", TRUE, NULL, GetState());
				}break;
				case 2:
				{
					if(IsMisfire())
						PlayHUDMotion("anm_idle_aim_jammed_2", TRUE, NULL, GetState());
					else
						PlayHUDMotion("anm_idle_aim_2", TRUE, NULL, GetState());
				}break;
			};
		}
	}
	else
	{
		switch (m_magazine.size())
		{
		case 0:
		{
			if(IsMisfire())
				PlayHUDMotion("anm_idle_jammed_0", TRUE, NULL, GetState());
			else
				PlayHUDMotion("anm_idle_0", TRUE, NULL, GetState());
		}break;
		case 1:
		{
			if(IsMisfire())
				PlayHUDMotion("anm_idle_jammed_1", TRUE, NULL, GetState());
			else
				PlayHUDMotion("anm_idle_1", TRUE, NULL, GetState());
		}break;
		case 2:
		{
			if(IsMisfire())
				PlayHUDMotion("anm_idle_jammed_2", TRUE, NULL, GetState());
			else
				PlayHUDMotion("anm_idle_2", TRUE, NULL, GetState());
		}break;
		};
	}
}
