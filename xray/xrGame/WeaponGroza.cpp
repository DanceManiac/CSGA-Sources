#include "pch_script.h"
#include "WeaponGroza.h"
#include "Scope.h"
#include "Silencer.h"
#include "GrenadeLauncher.h"
#include "HudManager.h"
#include "Actor.h"
#include "Level.h"

CWeaponGroza::CWeaponGroza() :CWeaponMagazined(SOUND_TYPE_WEAPON_SUBMACHINEGUN) 
{
}

CWeaponGroza::~CWeaponGroza() 
{
}

void CWeaponGroza::Load(LPCSTR section)
{
	inherited::Load		(section);
}

void CWeaponGroza::PlayAnimIdle()
{
	if(IsSilencerAttached())
	{
		if(IsZoomed())
			PlayAnimAim();
		else if (iAmmoElapsed == 0 && isHUDAnimationExist("anm_idle_empty_sil"))
			PlayHUDMotion("anm_idle_empty_sil", true, nullptr, GetState());
		else if (IsMisfire() && isHUDAnimationExist("anm_idle_jammed_sil") && !TryPlayAnimIdle())
			PlayHUDMotion("anm_idle_jammed_sil", true, nullptr, GetState());
		else
		{
			if (IsRotatingFromZoom())
			{
				if(iAmmoElapsed == 0 && isHUDAnimationExist("anm_idle_aim_end_empty_sil"))
					PlayHUDMotionNew("anm_idle_aim_end_empty_sil", true, GetState());
				else if(IsMisfire() && isHUDAnimationExist("anm_idle_aim_end_jammed_sil"))
					PlayHUDMotionNew("anm_idle_aim_end_jammed_sil", true, GetState());
				else
					if (isHUDAnimationExist("anm_idle_aim_end_sil"))
						PlayHUDMotionNew("anm_idle_aim_end_sil", true, GetState());
			}
		}
	}
	else
		inherited::PlayAnimIdle();
}

void CWeaponGroza::PlayAnimAim()
{
	if(IsSilencerAttached())
	{
		if (IsRotatingToZoom())
		{
			if (isHUDAnimationExist("anm_idle_aim_start_sil"))
			{
				if(iAmmoElapsed == 0 && isHUDAnimationExist("anm_idle_aim_start_empty_sil"))
					PlayHUDMotionNew("anm_idle_aim_start_empty_sil", true, GetState());
				else if(IsMisfire() && isHUDAnimationExist("anm_idle_aim_start_jammed_sil"))
					PlayHUDMotionNew("anm_idle_aim_start_jammed_sil", true, GetState());
				else
					PlayHUDMotionNew("anm_idle_aim_start_sil", true, GetState());
			}
		}

		if (const char* guns_aim_anm = GetAnimAimName())
		{
			string64 guns_aim_anm_sil;
			strconcat(sizeof(guns_aim_anm_sil), guns_aim_anm_sil, guns_aim_anm, IsSilencerAttached() ? "_sil" : "");
			if (isHUDAnimationExist(guns_aim_anm_sil))
			{
				PlayHUDMotionNew(guns_aim_anm_sil, true, GetState());
				return;
			}
		}

		if (iAmmoElapsed == 0 && isHUDAnimationExist("anm_idle_aim_empty_sil"))
			PlayHUDMotion("anm_idle_aim_empty_sil", true, nullptr, GetState());
		else if (IsMisfire() && isHUDAnimationExist("anm_idle_aim_jammed_sil"))
			PlayHUDMotion("anm_idle_aim_jammed_sil", true, nullptr, GetState());
		else
			if (isHUDAnimationExist("anm_idle_aim_sil"))
				PlayHUDMotion("anm_idle_aim_sil", true, nullptr, GetState());
	}
	else
		inherited::PlayAnimAim();
}

void CWeaponGroza::PlayAnimShow()
{
	if(IsSilencerAttached())
	{
		if(iAmmoElapsed == 0 && isHUDAnimationExist("anm_show_empty_sil"))
			PlayHUDMotion("anm_show_empty_sil", false, this, GetState());
		else if(IsMisfire() && isHUDAnimationExist("anm_show_jammed_sil"))
			PlayHUDMotion("anm_show_jammed_sil", false, this, GetState());
		else
			PlayHUDMotion("anm_show_sil", false, this, GetState());
	}
	else
		inherited::PlayAnimShow();
}

void CWeaponGroza::PlayAnimHide()
{
	if(IsSilencerAttached())
	{
		if(iAmmoElapsed == 0 && isHUDAnimationExist("anm_hide_empty_sil"))
			PlayHUDMotion("anm_hide_empty_sil", true, this, GetState());
		else if(IsMisfire() && isHUDAnimationExist("anm_hide_jammed_sil"))
			PlayHUDMotion("anm_hide_jammed_sil", true, this, GetState());
		else
			PlayHUDMotion("anm_hide_sil", true, this, GetState());
	}
	else
		inherited::PlayAnimHide();
}

void CWeaponGroza::PlayAnimReload()
{
	if(IsSilencerAttached())
	{
		if(iAmmoElapsed == 0 && isHUDAnimationExist ("anm_reload_empty_sil"))
			PlayHUDMotion("anm_reload_empty_sil", true, this, GetState());
		else if(IsMisfire() && isHUDAnimationExist("anm_reload_jammed_sil"))
			PlayHUDMotion("anm_reload_jammed_sil", true, this, GetState());
		else
			PlayHUDMotion("anm_reload_sil", true, this, GetState());
	}
	else
		inherited::PlayAnimReload();
}

void CWeaponGroza::PlayAnimIdleMoving()
{
	if(IsSilencerAttached())
	{
		if(iAmmoElapsed == 0 && isHUDAnimationExist("anm_idle_moving_empty_sil"))
			PlayHUDMotion("anm_idle_moving_empty_sil", true, nullptr, GetState());
		else if(IsMisfire() && isHUDAnimationExist("anm_idle_moving_jammed_sil"))
			PlayHUDMotion("anm_idle_moving_jammed_sil", true, nullptr, GetState());
		else
			if(isHUDAnimationExist("anm_idle_moving_sil"))
				PlayHUDMotion("anm_idle_moving_sil", true, nullptr, GetState());
	}
	else
		inherited::PlayAnimIdleMoving();
}

void CWeaponGroza::PlayAnimIdleSprint()
{
	if(IsSilencerAttached())
	{
		if(iAmmoElapsed == 0 && isHUDAnimationExist("anm_idle_sprint_empty_sil"))
			PlayHUDMotion("anm_idle_sprint_empty_sil", true, nullptr, GetState());
		else if(IsMisfire() && isHUDAnimationExist("anm_idle_sprint_jammed_sil"))
			PlayHUDMotion("anm_idle_sprint_jammed_sil", true, nullptr, GetState());
		else
			if(isHUDAnimationExist("anm_idle_sprint_sil"))
				PlayHUDMotion("anm_idle_sprint_sil", true, nullptr, GetState());
	}
	else
		inherited::PlayAnimIdleSprint();
}

using namespace luabind;

#pragma optimize("s",on)
void CWeaponGroza::script_register	(lua_State *L)
{
	module(L)
	[
		class_<CWeaponGroza,CGameObject>("CWeaponGroza")
			.def(constructor<>())
	];
}
