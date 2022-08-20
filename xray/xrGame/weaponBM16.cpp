#include "stdafx.h"
#include "weaponBM16.h"

CWeaponBM16::~CWeaponBM16()
{
}

void CWeaponBM16::Load	(LPCSTR section)
{
	inherited::Load		(section);
	m_sounds.LoadSound	(section, "snd_reload_1", "sndReload1", true, m_eSoundReload);
	m_sounds.LoadSound	(section, "snd_reload_jammed_last", "sndReloadJammedLast", true, m_eSoundReload);
}

void CWeaponBM16::PlayReloadSound()
{
	switch( m_magazine.size() )
	{
	case 1:
		if(IsMisfire())
			PlaySound	("sndReloadJammedLast",get_LastFP());
		else
			PlaySound	("sndReload1",get_LastFP());
		break;
	case 0:
		PlaySound	("sndReload",get_LastFP());
		break;
	}
}

void CWeaponBM16::PlayAnimShoot()
{
	switch( m_magazine.size() )
	{
	case 1:
		if(IsMisfire())
			PlayHUDMotion("anm_shoot_jammed_1",FALSE,this,GetState());
		else if(IsZoomed())
			PlayHUDMotion("anm_shoot_aim_1",FALSE,this,GetState());
		else if(IsZoomed() && IsMisfire())
			PlayHUDMotion("anm_shoot_aim_jammed_1",FALSE,this,GetState());
		else
			PlayHUDMotion("anm_shoot_1",FALSE,this,GetState());
		break;
	case 2:
		if(IsMisfire())
			PlayHUDMotion("anm_shoot_jammed_2",FALSE,this,GetState());
		else if(IsZoomed())
			PlayHUDMotion("anm_shoot_aim_2",FALSE,this,GetState());
		else if(IsZoomed() && IsMisfire())
			PlayHUDMotion("anm_shoot_aim_jammed_2",FALSE,this,GetState());
		else
			PlayHUDMotion("anm_shoot_2",FALSE,this,GetState());
		break;
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

void CWeaponBM16::PlayAnimBore()//в ганс двухстволках не юзаются анимы скуки
{
	switch( m_magazine.size() )
	{
	case 0:
		PlayHUDMotion("anm_bore_0",TRUE,this,GetState());
		break;
	case 1:
		PlayHUDMotion("anm_bore_1",TRUE,this,GetState());
		break;
	case 2:
		PlayHUDMotion("anm_bore_2",TRUE,this,GetState());
		break;
	}
}

void CWeaponBM16::PlayAnimReload()
{
	bool b_both = HaveCartridgeInInventory(2);

	VERIFY(GetState()==eReload);
	if(m_magazine.size()==1 || !b_both)
		if(IsMisfire())
			PlayHUDMotion("anm_reload_jammed_1",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_reload_1",TRUE,this,GetState());
	else
		if(IsMisfire())
			PlayHUDMotion("anm_reload_jammed_2",TRUE,this,GetState());
		else
			PlayHUDMotion("anm_reload_2",TRUE,this,GetState());
}

void  CWeaponBM16::PlayAnimIdleMoving()
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

void  CWeaponBM16::PlayAnimIdleSprint()
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

void CWeaponBM16::PlayAnimIdle()
{
	if(TryPlayAnimIdle())	return;

	if(IsZoomed())
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
