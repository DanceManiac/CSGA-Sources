#pragma once

#include "weaponcustompistol.h"
#include "script_export_space.h"

class CWeaponShotgun :	public CWeaponCustomPistol
{
	typedef CWeaponCustomPistol inherited;
public:
					CWeaponShotgun		();
	virtual			~CWeaponShotgun		();

	virtual void	Load				(LPCSTR section);
	virtual BOOL	net_Spawn			(CSE_Abstract* DC);
	virtual void	net_Destroy			();
	virtual void	net_Export			(NET_Packet& P);
	virtual void	net_Import			(NET_Packet& P);
	virtual void	OnShot				();

	virtual void	Reload				();
	virtual void	switch2_Fire		();
	void			switch2_StartReload ();
	void			switch2_AddCartgidge();
	void			switch2_EndReload	();

	virtual void	PlayAnimOpenWeapon	();
	virtual void	PlayAnimAddOneCartridgeWeapon();
	void			PlayAnimCloseWeapon	();

	virtual bool	Action(s32 cmd, u32 flags);

	virtual bool	SwitchAmmoType(u32 flags);

	bool			m_bAddCartridgeOpen; //говорим, что open содержит в себе добавление в магазин патрона
	bool			m_bEmptyPreloadMode; //после анимы anm_open_empty надо играть add_cartridge_preloaded или close_preloaded (т.к. в anm_open_empty загоняется патрон в патронник)
	bool			bPreloadAnimAdapter; //нужен чтобы после anm_open_empty была анимка add_cartridge_preloaded или close_preloaded
	bool			bStopReload;// флаг того что мы пытаемся остановить перезарядку

protected:
	virtual void	OnAnimationEnd		(u32 state);
	void			TriStateReload		();
	virtual void	OnStateSwitch		(u32 S);

	bool			HaveCartridgeInInventory(u8 cnt);
	virtual u8		AddCartridge		(u8 cnt);

	ESoundTypes		m_eSoundOpen;
	ESoundTypes		m_eSoundAddCartridge;
	ESoundTypes		m_eSoundClose;

	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CWeaponShotgun)
#undef script_type_list
#define script_type_list save_type_list(CWeaponShotgun)
