#include "stdafx.h"
#include "weaponpistol.h"
#include "ParticlesObject.h"
#include "actor.h"

CWeaponPistol::CWeaponPistol()
{
	SetPending(false);
}

CWeaponPistol::~CWeaponPistol(void)
{}

void CWeaponPistol::net_Destroy()
{
	inherited::net_Destroy();
}

void CWeaponPistol::Load(LPCSTR section)
{
	inherited::Load(section);
}

void CWeaponPistol::OnH_B_Chield()
{
	inherited::OnH_B_Chield();
}

void CWeaponPistol::PlayAnimShow()
{
	VERIFY(GetState() == eShowing);

	inherited::PlayAnimShow();
}

void CWeaponPistol::PlayAnimBore()
{
    VERIFY(GetState() == eBore);

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
	if (TryPlayAnimIdle())
		return;

	VERIFY(GetState() == eIdle);

	inherited::PlayAnimIdle();
}

void CWeaponPistol::PlayAnimAim()
{
	inherited::PlayAnimAim();
}

void CWeaponPistol::PlayAnimReload()
{	
	VERIFY(GetState() == eReload);

	inherited::PlayAnimReload();
}

void CWeaponPistol::PlayAnimIdleMovingCrouch()
{
	inherited::PlayAnimIdleMovingCrouch();
}

void CWeaponPistol::PlayAnimIdleMovingCrouchSlow()
{
	inherited::PlayAnimIdleMovingCrouchSlow();
}

void CWeaponPistol::PlayAnimIdleMovingSlow()
{
	inherited::PlayAnimIdleMovingSlow();
}

void CWeaponPistol::PlayAnimHide()
{
	VERIFY(GetState()==eHiding);

	inherited::PlayAnimHide();
}

void CWeaponPistol::PlayAnimShoot()
{
	VERIFY(GetState() == eFire);
	
	inherited::PlayAnimShoot();
}

void CWeaponPistol::switch2_Reload()
{
	inherited::switch2_Reload();
}

void CWeaponPistol::OnAnimationEnd(u32 state)
{
	inherited::OnAnimationEnd(state);
}

void CWeaponPistol::OnShot()
{
	inherited::OnShot();
}

void CWeaponPistol::UpdateSounds()
{
	inherited::UpdateSounds();
}