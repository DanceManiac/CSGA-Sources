#include "stdafx.h"
#include "weaponpistol.h"
#include "ParticlesObject.h"
#include "actor.h"

CWeaponPistol::CWeaponPistol()
{
	SetPending			(FALSE);
}

CWeaponPistol::~CWeaponPistol(void)
{
}

void CWeaponPistol::net_Destroy()
{
	inherited::net_Destroy();
}


void CWeaponPistol::Load	(LPCSTR section)
{
	inherited::Load		(section);

}

void CWeaponPistol::OnH_B_Chield		()
{
	inherited::OnH_B_Chield		();
}

void CWeaponPistol::PlayAnimShow	()
{
	VERIFY(GetState()==eShowing);

	inherited::PlayAnimShow();
}

void CWeaponPistol::PlayAnimBore()
{
	if(iAmmoElapsed==0 && isHUDAnimationExist("anm_bore_empty"))
		PlayHUDMotion	("anm_bore_empty", TRUE, this, GetState());
	else if(IsMisfire() && isHUDAnimationExist("anm_bore_jammed"))
		PlayHUDMotion	("anm_bore_jammed", TRUE, this, GetState());
	else
		inherited::PlayAnimBore();
}

void CWeaponPistol::PlayAnimIdleSprint()
{
	inherited::PlayAnimIdleSprint();
}

void CWeaponPistol::PlayAnimIdleMoving()
{
	inherited::PlayAnimIdleMoving();
}


void CWeaponPistol::PlayAnimIdle()
{
	if (TryPlayAnimIdle()) return;

	VERIFY(GetState()==eIdle);

	inherited::PlayAnimIdle		();
}

void CWeaponPistol::PlayAnimAim()
{
	inherited::PlayAnimAim();
}

void CWeaponPistol::PlayAnimReload()
{	
	VERIFY(GetState()==eReload);

	inherited::PlayAnimReload();
}


void CWeaponPistol::PlayAnimHide()
{
	VERIFY(GetState()==eHiding);

	inherited::PlayAnimHide();
}

void CWeaponPistol::PlayAnimShoot()
{
	VERIFY(GetState()==eFire);
	
    string_path guns_shoot_anm{};
    strconcat(sizeof(guns_shoot_anm), guns_shoot_anm, "anm_shoot", (this->IsZoomed() && !this->IsRotatingToZoom()) ? "_aim" : "", ((iAmmoElapsed == 1) ? "_last" : "" || (IsMisfire()) ? "_jammed" : ""), this->IsSilencerAttached() ? "_sil" : "");

    PlayHUDMotionNew(guns_shoot_anm, FALSE, GetState());
	
	if(isHUDAnimationExist("anm_shots"))//��� ������� ��������
	{
		PlayHUDMotion("anm_shots", FALSE, this, GetState());
	}
	if(isHUDAnimationExist("anm_shots_l"))
	{
		PlayHUDMotion("anm_shots_l", FALSE, this, GetState());
	}
}

void CWeaponPistol::switch2_Reload()
{
	inherited::switch2_Reload();
}

void CWeaponPistol::OnAnimationEnd(u32 state)
{
	inherited::OnAnimationEnd(state);
}

void CWeaponPistol::OnShot		()
{
	PlaySound		(m_sSndShotCurrent.c_str(),get_LastFP());

	AddShotEffector	();
	
	PlayAnimShoot	();

	// Shell Drop
	Fvector vel; 
	PHGetLinearVell(vel);
	if(!bMisfire)
	{
		OnShellDrop					(get_LastSP(),  vel);
	}

	// ����� �� ������
	
	StartFlameParticles	();
	R_ASSERT2(!m_pFlameParticles || !m_pFlameParticles->IsLooped(),
			  "can't set looped particles system for shoting with pistol");
	
	//��� �� ������
	StartSmokeParticles	(get_LastFP(), vel);
}

void CWeaponPistol::UpdateSounds()
{
	inherited::UpdateSounds();
}