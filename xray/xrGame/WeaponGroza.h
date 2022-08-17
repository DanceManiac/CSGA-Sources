#pragma once

//#include "WeaponMagazined.h"
#include "WeaponMagazinedWGrenade.h"
#include "script_export_space.h"
#include "Weapon.h"
#include "HudSound.h"
#include "ai_sounds.h"

class CWeaponGroza :
	/*public CWeaponMagazined,*/ public CWeaponMagazinedWGrenade
{
	typedef CWeaponMagazinedWGrenade inherited;
public:
				CWeaponGroza();
	virtual		~CWeaponGroza();
	
	virtual void	Load			(LPCSTR section);
	virtual bool	SwitchMode		();
protected:
	virtual void	PlayAnimModeSwitch	();
public:
	virtual void	PlayAnimShow		();
	virtual void	PlayAnimHide		();
	virtual void	PlayAnimReload		();
	virtual void	PlayAnimIdle		();
	virtual void	PlayAnimAim			();
	virtual void	PlayAnimIdleMoving	();
	virtual void	PlayAnimIdleSprint	();
	virtual bool	CanAttach(PIItem pIItem);

	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CWeaponGroza)
#undef script_type_list
#define script_type_list save_type_list(CWeaponGroza)
