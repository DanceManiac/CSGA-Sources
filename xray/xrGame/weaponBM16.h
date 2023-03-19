#pragma once

#include "weaponShotgun.h"
#include "script_export_space.h"

class CWeaponBM16 :public CWeaponShotgun
{
	typedef CWeaponShotgun inherited;

public:
	virtual			~CWeaponBM16					();
	virtual void	Load							(LPCSTR section);

private:
	string64 guns_aim_anm_bm;
protected:
	const char*		GetAnimAimName					();
	virtual void	PlayAnimShoot					();
	virtual void	PlayAnimReload					();
	virtual void	PlayReloadSound					();
	virtual void	PlayAnimIdle					();
	virtual void	PlayAnimIdleMoving				();
	virtual void	PlayAnimIdleSprintStart			();
	virtual void	PlayAnimIdleSprint				();
	virtual void	PlayAnimIdleSprintEnd			();
	virtual void	PlayAnimShow					();
	virtual void	PlayAnimHide					();
	virtual void	PlayAnimIdleMovingSlow			();
	virtual void	PlayAnimIdleMovingCrouchSlow	();
	virtual void	PlayAnimIdleMovingCrouch		();
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CWeaponBM16)
#undef script_type_list
#define script_type_list save_type_list(CWeaponBM16)
