#include "stdafx.h"
#include "weaponshotgun.h"
#include "entity.h"
#include "ParticlesObject.h"
#include "xr_level_controller.h"
#include "inventory.h"
#include "level.h"
#include "actor.h"

CWeaponShotgun::CWeaponShotgun()
{
	m_eSoundClose			= ESoundTypes(SOUND_TYPE_WEAPON_SHOOTING);
	m_eSoundAddCartridge	= ESoundTypes(SOUND_TYPE_WEAPON_SHOOTING);
}

CWeaponShotgun::~CWeaponShotgun()
{}

BOOL CWeaponShotgun::net_Spawn(CSE_Abstract* DC)
{
	BOOL r = inherited::net_Spawn(DC);

	CSE_ALifeItemWeaponShotGun* _dc = smart_cast<CSE_ALifeItemWeaponShotGun*>(DC);
	xr_vector<u8> ammo_ids = _dc->m_AmmoIDs;

	for (u32 i = 0; i < ammo_ids.size(); i++)
	{
		u8 LocalAmmoType = ammo_ids[i];
		if (i >= m_magazine.size()) continue;
		CCartridge& l_cartridge = *(m_magazine.begin() + i);
		if (LocalAmmoType == l_cartridge.m_LocalAmmoType) continue;
		l_cartridge.Load(*m_ammoTypes[LocalAmmoType], LocalAmmoType);
	}

	return r;
}

void CWeaponShotgun::net_Destroy()
{
	inherited::net_Destroy();
}

void CWeaponShotgun::Load(LPCSTR section)
{
	inherited::Load(section);

	if(pSettings->line_exist(section, "tri_state_reload"))
		m_bTriStateReload = !!pSettings->r_bool(section, "tri_state_reload");

	if(m_bTriStateReload)
	{
		m_sounds.LoadSound(section, "snd_open_weapon", "sndOpen", false, m_eSoundOpen);
		m_sounds.LoadSound(section, "snd_add_cartridge", "sndAddCartridge", false, m_eSoundAddCartridge);
		m_sounds.LoadSound(section, "snd_add_cartridge_empty", "sndAddCartridgeEmpty", false, m_eSoundAddCartridge);
		m_sounds.LoadSound(section, "snd_close_weapon", "sndClose", false, m_eSoundClose);
		m_sounds.LoadSound(section, "snd_close_weapon_empty", "sndCloseEmpty", false, m_eSoundClose);

		if(pSettings->line_exist(hud_sect, "add_cartridge_in_open"))
		m_bAddCartridgeOpen = !!pSettings->r_bool(hud_sect, "add_cartridge_in_open");

		if(pSettings->line_exist(hud_sect, "empty_preload_mode"))
		m_bEmptyPreloadMode = !!pSettings->r_bool(hud_sect, "empty_preload_mode");

		if (m_bAddCartridgeOpen)
			m_sounds.LoadSound(section, "snd_open_weapon_empty", "sndOpenEmpty", false, m_eSoundOpen);

		if (m_bEmptyPreloadMode)
		{
			m_sounds.LoadSound(section, "snd_add_cartridge_preloaded", "sndAddCartridgePreloaded", false, m_eSoundOpen);
			m_sounds.LoadSound(section, "snd_close_weapon_preloaded", "sndClosePreloaded", false, m_eSoundClose);
		}

		m_sounds.LoadSound(section, "snd_breechblock", "sndPump", false, m_eSoundReload);
	};

}

void CWeaponShotgun::switch2_Fire()
{
	inherited::switch2_Fire();
	bWorking = false;
}

void CWeaponShotgun::OnShot()
{
	inherited::OnShot();

	if (!IsMisfire())
		PlaySound("sndPump", get_LastFP());
	else
		PlaySound("sndJam", get_LastFP());
}

bool CWeaponShotgun::Action(s32 cmd, u32 flags) 
{
	if(inherited::Action(cmd, flags)) return true;

	if(m_bTriStateReload && GetState() == eReload && !bStopReload && cmd == kWPN_FIRE && flags&CMD_START && (m_sub_state == eSubstateReloadInProcess || m_sub_state == eSubstateReloadBegin))//остановить перезарядку
	{
		bStopReload = true;
		return true;
	}

	return false;
}

bool CWeaponShotgun::SwitchAmmoType(u32 flags)
{
	if(IsTriStateReload() && iAmmoElapsed == iMagazineSize)
		return false;

	return inherited::SwitchAmmoType(flags);
}

void CWeaponShotgun::OnAnimationEnd(u32 state) 
{
	if(!m_bTriStateReload || state != eReload || IsMisfire())
	{
        bStopReload = false;
		bPreloadAnimAdapter = false;
		return inherited::OnAnimationEnd(state);
	}

	switch(m_sub_state)
	{
		case eSubstateReloadBegin:
		{
			if (bStopReload || iAmmoElapsed == iMagazineSize)
			{
				m_sub_state = eSubstateReloadEnd;
				SwitchState(eReload);
			}
			else
			{
				m_sub_state = eSubstateReloadInProcess;
				SwitchState(eReload);
			}
		}break;
		case eSubstateReloadInProcess:
		{
			if(0 != AddCartridge(1) || bStopReload)
				m_sub_state = eSubstateReloadEnd;

			SwitchState(eReload);
		}break;
		case eSubstateReloadEnd:
		{
			bStopReload = false;
			m_sub_state = eSubstateReloadBegin;
			SwitchState(eIdle);
		}break;
		
	};
}

void CWeaponShotgun::Reload() 
{
	if(m_bTriStateReload)
		TriStateReload();
	else
		inherited::Reload();
}

void CWeaponShotgun::TriStateReload()
{
	if(!HaveCartridgeInInventory(1))
		return;

	m_sub_state	= eSubstateReloadBegin;
	SwitchState(eReload);
}

void CWeaponShotgun::OnStateSwitch(u32 S)
{
	if(!m_bTriStateReload || S != eReload || IsMisfire())
	{
        bStopReload = false;
		bPreloadAnimAdapter = false;
		inherited::OnStateSwitch(S);
		return;
	}

	CWeapon::OnStateSwitch(S);

	if(m_magazine.size() == (u32)iMagazineSize || !HaveCartridgeInInventory(1))
	{
		switch2_EndReload();
		m_sub_state = eSubstateReloadEnd;
		return;
	};

	switch (m_sub_state)
	{
	case eSubstateReloadBegin:
	{
		if(HaveCartridgeInInventory(1))
		{
			switch2_StartReload();
			if(iAmmoElapsed == 0 && m_bAddCartridgeOpen || !bPreloadAnimAdapter)
				AddCartridge(1);
		}
	}break;
	case eSubstateReloadInProcess:
	{
		if(HaveCartridgeInInventory(1))
			switch2_AddCartgidge();
	}break;
	case eSubstateReloadEnd:
		switch2_EndReload();
	break;
	};
}

void CWeaponShotgun::switch2_StartReload()
{
    if (m_bAddCartridgeOpen && iAmmoElapsed == 0)
        PlaySound("sndOpenEmpty", get_LastFP());
	else
		PlaySound("sndOpen", get_LastFP());

	PlayAnimOpenWeapon();
	SetPending(true);
}

void CWeaponShotgun::switch2_AddCartgidge()
{
	if (!m_bAddCartridgeOpen && iAmmoElapsed == 0)
		PlaySound("sndAddCartridgeEmpty", get_LastFP());
	else if (m_bEmptyPreloadMode && bPreloadAnimAdapter)
		PlaySound("sndAddCartridgePreloaded", get_LastFP());
	else
		PlaySound("sndAddCartridge", get_LastFP());

	PlayAnimAddOneCartridgeWeapon();
	SetPending(true);
}

void CWeaponShotgun::switch2_EndReload()
{
	SetPending(true);

	if (!m_bAddCartridgeOpen && iAmmoElapsed == 0)
		PlaySound("sndCloseEmpty", get_LastFP());
	else if (m_bEmptyPreloadMode && bPreloadAnimAdapter)
		PlaySound("sndClosePreloaded", get_LastFP());
	else
		PlaySound("sndClose", get_LastFP());

	PlayAnimCloseWeapon();
}

void CWeaponShotgun::PlayAnimOpenWeapon()
{
	VERIFY(GetState()==eReload);

	if(m_bEmptyPreloadMode && iAmmoElapsed == 0)
	{
		PlayHUDMotion("anm_open_empty", false, this, GetState());
		bPreloadAnimAdapter = true;
	}
	else
		PlayHUDMotion("anm_open", false, this, GetState());
}

void CWeaponShotgun::PlayAnimAddOneCartridgeWeapon()
{
	VERIFY(GetState()==eReload);

	if(m_bEmptyPreloadMode && bPreloadAnimAdapter)
	{
		if(iAmmoElapsed == 0)
			PlayHUDMotion("anm_add_cartridge_empty_preloaded", false, this, GetState());
		else
			PlayHUDMotion("anm_add_cartridge_preloaded", false, this, GetState());

		bPreloadAnimAdapter = false;
	}
	else if (!m_bAddCartridgeOpen && iAmmoElapsed == 0)
		PlayHUDMotion("anm_add_cartridge_empty", false, this, GetState());
	else
		PlayHUDMotion("anm_add_cartridge", false, this, GetState());
}

void CWeaponShotgun::PlayAnimCloseWeapon()
{
	VERIFY(GetState()==eReload);

	if (m_bEmptyPreloadMode && bPreloadAnimAdapter)
	{
		if(iAmmoElapsed == 0)
			PlayHUDMotion("anm_add_cartridge_empty_preloaded", false, this, GetState());
		else
			PlayHUDMotion("anm_close_preloaded", false, this, GetState());

		bPreloadAnimAdapter = false;
	}
	else if (!m_bAddCartridgeOpen && iAmmoElapsed == 0)
		PlayHUDMotion("anm_add_cartridge_empty", false, this, GetState());
	else
		PlayHUDMotion("anm_close", false, this, GetState());
}

bool CWeaponShotgun::HaveCartridgeInInventory(u8 cnt)
{
	if (unlimited_ammo())
		return true;

	m_pAmmo = NULL;
	if(m_pInventory) 
	{
		//попытаться найти в инвентаре патроны текущего типа 
		m_pAmmo = smart_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[m_ammoType]));
		
		if(!m_pAmmo )
		{
			for(u32 i = 0; i < m_ammoTypes.size(); ++i) 
			{
				//проверить патроны всех подходящих типов
				m_pAmmo = smart_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[i]));
				if(m_pAmmo) 
				{ 
					m_ammoType = i; 
					break; 
				}
			}
		}
	}
	return (m_pAmmo!=NULL)&&(m_pAmmo->m_boxCurr>=cnt) ;
}

u8 CWeaponShotgun::AddCartridge(u8 cnt)
{
	if(m_set_next_ammoType_on_reload != u32(-1)){
		m_ammoType						= m_set_next_ammoType_on_reload;
		m_set_next_ammoType_on_reload	= u32(-1);

	}

	if(!HaveCartridgeInInventory(1))
		return 0;

	VERIFY((u32)iAmmoElapsed == m_magazine.size());


	if (m_DefaultCartridge.m_LocalAmmoType != m_ammoType)
		m_DefaultCartridge.Load(*m_ammoTypes[m_ammoType], u8(m_ammoType));
	CCartridge l_cartridge = m_DefaultCartridge;
	while(cnt)// && m_pAmmo->Get(l_cartridge)) 
	{
		if (!unlimited_ammo())
		{
			if (!m_pAmmo->Get(l_cartridge)) break;
		}
		--cnt;
		++iAmmoElapsed;
		l_cartridge.m_LocalAmmoType = u8(m_ammoType);
		m_magazine.push_back(l_cartridge);
//		m_fCurrentCartirdgeDisp = l_cartridge.m_kDisp;
	}
	m_ammoName = (m_pAmmo) ? m_pAmmo->m_nameShort : NULL;

	VERIFY((u32)iAmmoElapsed == m_magazine.size());

	//выкинуть коробку патронов, если она пустая
	if(m_pAmmo && !m_pAmmo->m_boxCurr && OnServer()) 
		m_pAmmo->SetDropManual(TRUE);

	return cnt;
}

void CWeaponShotgun::net_Export(NET_Packet& P)
{
	inherited::net_Export(P);	
	P.w_u8(u8(m_magazine.size()));	
	for (u32 i=0; i<m_magazine.size(); i++)
	{
		CCartridge& l_cartridge = *(m_magazine.begin()+i);
		P.w_u8(l_cartridge.m_LocalAmmoType);
	}
}

void CWeaponShotgun::net_Import(NET_Packet& P)
{
	inherited::net_Import(P);	
	u8 AmmoCount = P.r_u8();
	for (u32 i=0; i<AmmoCount; i++)
	{
		u8 LocalAmmoType = P.r_u8();
		if (i>=m_magazine.size()) continue;
		CCartridge& l_cartridge = *(m_magazine.begin()+i);
		if (LocalAmmoType == l_cartridge.m_LocalAmmoType) continue;
#ifdef DEBUG
		Msg("! %s reload to %s", *l_cartridge.m_ammoSect, *(m_ammoTypes[LocalAmmoType]));
#endif
		l_cartridge.Load(*(m_ammoTypes[LocalAmmoType]), LocalAmmoType); 
	}
}
