#include "pch_script.h"
#include "hudmanager.h"
#include "WeaponMagazined.h"
#include "Entity.h"
#include "Actor.h"
#include "ParticlesObject.h"
#include "Scope.h"
#include "Silencer.h"
#include "GrenadeLauncher.h"
#include "Inventory.h"
#include "xrserver_objects_alife_items.h"
#include "ActorEffector.h"
#include "EffectorZoomInertion.h"
#include "xr_level_controller.h"
#include "Level.h"
#include "object_broker.h"
#include "string_table.h"
#include "MPPlayersBag.h"
#include "ui/UIXmlInit.h"
#include "ui/UIWindow.h"
#include "HudSound.h"
#include "CustomDetector.h"
#include "game_cl_single.h"
#include "WeaponBinoculars.h"
#include "WeaponKnife.h"
#include "WeaponMagazinedWGrenade.h"
#include "WeaponBM16.h"

ENGINE_API bool	g_dedicated_server;

CUIXml* pWpnScopeXml = nullptr;

void createWpnScopeXML()
{
	if(!pWpnScopeXml)
	{
		pWpnScopeXml = xr_new<CUIXml>();
		pWpnScopeXml->Load(CONFIG_PATH, UI_PATH, "scopes.xml");
	}
}

CWeaponMagazined::CWeaponMagazined(ESoundTypes eSoundType) : CWeapon()
{
	m_eSoundShow = ESoundTypes(SOUND_TYPE_ITEM_TAKING | eSoundType);
	m_eSoundHide = ESoundTypes(SOUND_TYPE_ITEM_HIDING | eSoundType);
	m_eSoundShot = ESoundTypes(SOUND_TYPE_WEAPON_SHOOTING | eSoundType);
	m_eSoundEmptyClick = ESoundTypes(SOUND_TYPE_WEAPON_EMPTY_CLICKING | eSoundType);
	m_eSoundReload = ESoundTypes(SOUND_TYPE_WEAPON_RECHARGING | eSoundType);
	
	m_sSndShotCurrent = nullptr;
	m_sSilencerFlameParticles = m_sSilencerSmokeParticles = nullptr;

	m_bFireSingleShot = false;
	m_iShotNum = 0;
	m_iQueueSize = WEAPON_ININITE_QUEUE;
	m_bLockType = false;
}

CWeaponMagazined::~CWeaponMagazined()
{}


void CWeaponMagazined::net_Destroy()
{
	inherited::net_Destroy();
}

//AVO: for custom added sounds check if sound exists
bool CWeaponMagazined::WeaponSoundExist(LPCSTR section, LPCSTR sound_name)
{
	LPCSTR str;
	bool sec_exist = process_if_exists_set(section, sound_name, &CInifile::r_string, str, true);
	if (sec_exist)
		return true;
	else
	{
#ifdef DEBUG
        Msg("~ [WARNING] ------ Sound [%s] does not exist in [%s]", sound_name, section);
#endif
		return false;
	}
}

//-AVO

void CWeaponMagazined::Load	(LPCSTR section)
{
	inherited::Load		(section);
		
	// Sounds
	m_sounds.LoadSound(section, "snd_draw", "sndShow", false, m_eSoundShow);
	m_sounds.LoadSound(section, "snd_holster", "sndHide", false, m_eSoundHide);

	//Alundaio: LAYERED_SND_SHOOT
	m_layered_sounds.LoadSound(section, "snd_shoot", "sndShot", false, m_eSoundShot);
	if (WeaponSoundExist(section, "snd_shoot_actor"))
		m_layered_sounds.LoadSound(section, "snd_shoot_actor", "sndShotActor", false, m_eSoundShot);

	m_sounds.LoadSound(section, "snd_empty", "sndEmptyClick", false, m_eSoundEmptyClick);
	m_sounds.LoadSound(section, "snd_reload", "sndReload", true, m_eSoundReload);
	m_sounds.LoadSound(section, "snd_reload_empty", "sndReloadEmpty", true, m_eSoundReload);
	m_sounds.LoadSound(section, "snd_reload_jammed", "sndReloadJammed", true, m_eSoundReload);
	m_sounds.LoadSound(section, "snd_reload_jammed_last", "sndReloadJammedLast", true, m_eSoundReload);
    m_sounds.LoadSound(section, "snd_changecartridgetype", "sndAmmoChange", true, m_eSoundReload);

	m_sounds.LoadSound(section, "snd_aim_start", "sndAimStart", false, m_eSoundShow);
    m_sounds.LoadSound(section, "snd_aim_end", "sndAimEnd", false, m_eSoundHide);

	if (m_bUseLowAmmoSnd)
        m_sounds.LoadSound(section, "snd_lowammo", "sndLowAmmo", false, m_eSoundShot);

	if (m_bUseLightMisfire)
		m_sounds.LoadSound(section, "snd_light_misfire", "sndLightMis", true, m_eSoundShot);

	if (m_bJamNotShot)
		m_sounds.LoadSound(section, "snd_jam", "sndJam", false, m_eSoundShot);

	m_sSndShotCurrent = "sndShot";
		
	//звуки и партиклы глушителя, еслит такой есть
	if (m_eSilencerStatus == ALife::eAddonAttachable || m_eSilencerStatus == ALife::eAddonPermanent)
	{
		if(pSettings->line_exist(section, "silencer_flame_particles"))
			m_sSilencerFlameParticles = pSettings->r_string(section, "silencer_flame_particles");
		if(pSettings->line_exist(section, "silencer_smoke_particles"))
			m_sSilencerSmokeParticles = pSettings->r_string(section, "silencer_smoke_particles");
		
		//Alundaio: LAYERED_SND_SHOOT Silencer
		m_layered_sounds.LoadSound(section, "snd_silncer_shot", "sndSilencerShot", false, m_eSoundShot);
		if (WeaponSoundExist(section, "snd_silncer_shot_actor"))
			m_layered_sounds.LoadSound(section, "snd_silncer_shot_actor", "sndSilencerShotActor", false, m_eSoundShot);
		//-Alundaio
	}

	if (pSettings->line_exist(section, "dispersion_start"))
		m_iShootEffectorStart = pSettings->r_u8(section, "dispersion_start");
	else
		m_iShootEffectorStart = 0;

	if (pSettings->line_exist(section, "fire_modes"))
	{
		m_bHasDifferentFireModes = true;
		shared_str FireModesList = pSettings->r_string(section, "fire_modes");
		int ModesCount = _GetItemCount(FireModesList.c_str());
		m_aFireModes.clear();
		
		for (int i=0; i<ModesCount; i++)
		{
			string16 sItem;
			_GetItem(FireModesList.c_str(), i, sItem);
			m_aFireModes.push_back	((s8)atoi(sItem));
		}
		
		m_iCurFireMode = ModesCount - 1;
		m_iPrefferedFireMode = READ_IF_EXISTS(pSettings, r_s16,section,"preffered_fire_mode",-1);
	}
	else
	{
		m_bHasDifferentFireModes = false;
	}
	LoadSilencerKoeffs();
}

void CWeaponMagazined::FireStart()
{
	if(!IsMisfire())
	{
		if(IsValid()) 
		{
			if(!IsWorking() || AllowFireWhileWorking())
			{
				if(GetState()==eReload) return;
				if(GetState()==eShowing) return;
				if(GetState()==eHiding) return;
				if(GetState()==eMisfire) return;

				inherited::FireStart();

				R_ASSERT(H_Parent());
				if (m_bUseLightMisfire && !IsGrenadeLauncherMode() && CheckForLightMisfire())
					SwitchState(eUnLightMis);
				else
					SwitchState(eFire);
			}
		}
		else 
		{
			if(GetState() == eIdle)
			{
				MsgGunEmpty();//сообщение о том, что магазин пуст
				SwitchState(eEmpty);
			}
		}
	}
	else
	{//misfire
		if(GetState() == eIdle && !IsGrenadeLauncherMode())
			SwitchState(eLookMis);
	}
}

void CWeaponMagazined::MsgGunEmpty()
{
	if(smart_cast<CActor*>(this->H_Parent()) && (Level().CurrentViewEntity()==H_Parent()))
        HUD().GetUI()->AddInfoMessage("gun_empty");
}

void CWeaponMagazined::FireEnd() 
{
	inherited::FireEnd();
}

void CWeaponMagazined::Reload() 
{
	inherited::Reload();
	TryReload();
}

#include "game_object_space.h"
#include "script_callback_ex.h"
#include "script_game_object.h"

bool CWeaponMagazined::TryReload() 
{
    CWeaponBinoculars* binoc = smart_cast<CWeaponBinoculars*>(m_pInventory->ActiveItem());
	if(m_pInventory && !binoc) 
	{
		if(IsGameTypeSingle() && ParentIsActor())
		{
			int	AC = GetSuitableAmmoTotal();
			Actor()->callback(GameObject::eWeaponNoAmmoAvailable)(lua_game_object(), AC);
		}
		if (m_ammoType < m_ammoTypes.size())
			m_pAmmo = smart_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[m_ammoType]));
		else
			m_pAmmo = nullptr;

		if(IsMisfire() && iAmmoElapsed)
		{
			SetPending(true);
			SwitchState(eReload); 
			return true;
		}

		if(m_pAmmo || unlimited_ammo())  
		{
			SetPending(true);
			SwitchState(eReload); 
			return true;
		} 
		else for(u32 i = 0; i < m_ammoTypes.size(); ++i) 
		{
			m_pAmmo = smart_cast<CWeaponAmmo*>(m_pInventory->GetAny( *m_ammoTypes[i] ));
			if(m_pAmmo) 
			{ 
				m_set_next_ammoType_on_reload = i;
				SetPending(true);
				SwitchState(eReload);
				return true;
			}
		}

	}
	
	if(GetState()==eIdle)
		SwitchState(eIdle);

	return false;
}

bool CWeaponMagazined::IsAmmoAvailable()
{
	if (smart_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[m_ammoType])))
		return	(true);
	else
		for(u32 i = 0; i < m_ammoTypes.size(); ++i)
			if (smart_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[i])))
				return	(true);
	return		(false);
}

void CWeaponMagazined::OnMagazineEmpty() 
{
	inherited::OnMagazineEmpty();
}

void CWeaponMagazined::UnloadMagazine(bool spawn_ammo)
{
	xr_map<LPCSTR, u16> l_ammo;
	
	while(!m_magazine.empty()) 
	{
		CCartridge &l_cartridge = m_magazine.back();
		xr_map<LPCSTR, u16>::iterator l_it;
		for(l_it = l_ammo.begin(); l_ammo.end() != l_it; ++l_it) 
		{
            if(!xr_strcmp(*l_cartridge.m_ammoSect, l_it->first)) 
            { 
				 ++(l_it->second); 
				 break; 
			}
		}

		if(l_it == l_ammo.end()) l_ammo[*l_cartridge.m_ammoSect] = 1;
		m_magazine.pop_back(); 
		--iAmmoElapsed;
	}

	VERIFY((u32)iAmmoElapsed == m_magazine.size());
	
	if (!spawn_ammo)
		return;

	xr_map<LPCSTR, u16>::iterator l_it;
	for(l_it = l_ammo.begin(); l_ammo.end() != l_it; ++l_it) 
	{
		CWeaponAmmo *l_pA = smart_cast<CWeaponAmmo*>(m_pInventory->GetAny(l_it->first));
		if(l_pA) 
		{
			u16 l_free = l_pA->m_boxSize - l_pA->m_boxCurr;
			l_pA->m_boxCurr = l_pA->m_boxCurr + (l_free < l_it->second ? l_free : l_it->second);
			l_it->second = l_it->second - (l_free < l_it->second ? l_free : l_it->second);
		}
		if(l_it->second && !unlimited_ammo()) SpawnAmmo(l_it->second, l_it->first);
	}
	
	if (!IsGrenadeLauncherMode() && IsMisfire())
        bMisfire = false;

	if(GetState() == eIdle)
		SwitchState(eIdle);//чтобы обновлялся худ после анлода, и проигрывалась анимация anm_idle_empty. Спасибо Валерку.
}

void CWeaponMagazined::ReloadMagazine() 
{
	m_dwAmmoCurrentCalcFrame = 0;	

	if (!m_bLockType) {
		m_ammoName = NULL;
		m_pAmmo = nullptr;
	}
	
	if (!m_pInventory) return;

	if(m_set_next_ammoType_on_reload != u32(-1)){		
		m_ammoType = m_set_next_ammoType_on_reload;
		m_set_next_ammoType_on_reload = u32(-1);
	}
	
	if(!unlimited_ammo()) 
	{
		//попытаться найти в инвентаре патроны текущего типа 
		m_pAmmo = smart_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[m_ammoType]));
		
		if(!m_pAmmo && !m_bLockType) 
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
	else
		m_ammoType = m_ammoType;


	//нет патронов для перезарядки
	if(!m_pAmmo && !unlimited_ammo() ) return;

	//разрядить магазин, если загружаем патронами другого типа
	if(!m_bLockType && !m_magazine.empty() && 
		(!m_pAmmo || xr_strcmp(m_pAmmo->cNameSect(), 
					 *m_magazine.back().m_ammoSect)))
		UnloadMagazine();

	VERIFY((u32)iAmmoElapsed == m_magazine.size());

	if (m_DefaultCartridge.m_LocalAmmoType != m_ammoType)
		m_DefaultCartridge.Load(*m_ammoTypes[m_ammoType], u8(m_ammoType));
	CCartridge l_cartridge = m_DefaultCartridge;
	while(iAmmoElapsed < iMagazineSize)
	{
		if (!unlimited_ammo())
		{
			if (!m_pAmmo->Get(l_cartridge)) break;
		}
		++iAmmoElapsed;
		l_cartridge.m_LocalAmmoType = u8(m_ammoType);
		m_magazine.push_back(l_cartridge);
	}
	m_ammoName = (m_pAmmo) ? m_pAmmo->m_nameShort : NULL;

	VERIFY((u32)iAmmoElapsed == m_magazine.size());

	//выкинуть коробку патронов, если она пустая
	if(m_pAmmo && !m_pAmmo->m_boxCurr && OnServer()) 
		m_pAmmo->SetDropManual(true);

	if(iMagazineSize > iAmmoElapsed) 
	{ 
		m_bLockType = true; 
		ReloadMagazine(); 
		m_bLockType = false; 
	}

	VERIFY((u32)iAmmoElapsed == m_magazine.size());
}

void CWeaponMagazined::OnStateSwitch	(u32 S)
{
	u32 old_state = GetState();
	inherited::OnStateSwitch(S);
        switch (S) {
        case eIdle:
            switch2_Idle();
            break;
        case eZoomStart: {
            PlaySound("sndAimStart", get_LastSP());
            PlayAnimAimStart();
            SetPending(false);
        } break;
        case eZoomEnd: {
            PlaySound("sndAimEnd", get_LastSP());
            PlayAnimAimEnd();
            SetPending(false);
        }
        case eFire:
            switch2_Fire();
            break;
        case eMisfire: break;
        case eEmpty:
            EmptyMove();
            break;
        case eLookMis:
			{
				switch2_LookMisfire();
			} break;
        case eUnLightMis: {
				switch2_LightMisfire();
			} break;
        case eReload:
            switch2_Reload();
            break;
        case eShowing:
            switch2_Showing();
            break;
        case eShowingDet: {
            PlayAnimShowDet();
            SetPending(true);
        }break;
        case eShowingEndDet: {
            PlayAnimShowEndDet();
            SetPending(true);
        }break;
        case eHiding:
		{
            if (old_state != eHiding)
                switch2_Hiding();
        }
        break;
        case eHideDet:
		{
            PlayAnimHideDet();
            SetPending(true);
        } break;
		case eHidden:
			switch2_Hidden();
			break;
		}
}

void CWeaponMagazined::UpdateCL()
{
	inherited::UpdateCL	();
	float dt = Device.fTimeDelta;

	

	//когда происходит апдейт состояния оружия
	//ничего другого не делать
	if(GetNextState() == GetState())
	{
		switch (GetState())
		{
		case eShowing:
		case eShowingDet:
        case eShowingEndDet:
		case eHiding:
        case eHideDet:
		case eZoomStart:
		case eZoomEnd:
		case eReload:
		case eIdle:
			{
				fShotTimeCounter -=	dt;
				clamp (fShotTimeCounter, 0.0f, flt_max);
			}break;
		case eFire:			
			{
				state_Fire(dt);
			}break;
		case eMisfire: state_Misfire(dt); break;
		case eEmpty: break;
		case eLookMis: break;
		case eUnLightMis: break;
		case eHidden: break;
		}
	}

	UpdateSounds();
}

void CWeaponMagazined::UpdateSounds()
{
	if (Device.dwFrame == dwUpdateSounds_Frame)  
		return;
	
	dwUpdateSounds_Frame = Device.dwFrame;

	Fvector P = get_LastFP();
	m_sounds.SetPosition("sndShow", P);
	m_sounds.SetPosition("sndHide", P);
	m_sounds.SetPosition("sndReload", P);
    m_layered_sounds.SetPosition("sndShot", P);
	m_layered_sounds.SetPosition("sndShotActor", P);
}

void CWeaponMagazined::state_Fire(float dt)
{
	if(iAmmoElapsed > 0)
	{
		VERIFY(fOneShotTime>0.f);

		Fvector p1 {}, d {};
		p1.set(get_LastFP());
		d.set(get_LastFD());

		if (!H_Parent()) return;
		if (smart_cast<CMPPlayersBag*>(H_Parent()) != nullptr)
		{
			Msg("! WARNING: state_Fire of object [%d][%s] while parent is CMPPlayerBag...", ID(), cNameSect().c_str());
			return;
		}

		CInventoryOwner* io	= smart_cast<CInventoryOwner*>(H_Parent());
		if(nullptr == io->inventory().ActiveItem())
		{
				Log("current_state", GetState());
				Log("next_state", GetNextState());
				Log("item_sect", cNameSect().c_str());
				Log("H_Parent", H_Parent()->cNameSect().c_str());
		}

		CEntity* E = smart_cast<CEntity*>(H_Parent());
		E->g_fireParams	(this, p1,d);

		if(!E->g_stateFire())
			StopShooting();

		if (m_iShotNum == 0)
		{
			m_vStartPos = p1;
			m_vStartDir = d;
		};
		
		VERIFY(!m_magazine.empty());

		while (!m_magazine.empty() && fShotTimeCounter<0 && (IsWorking() || m_bFireSingleShot) && (m_iQueueSize<0 || m_iShotNum<m_iQueueSize))
		{
			if(m_bJamNotShot && CheckForMisfire())
			{
				OnShotJammed();
				return;
			}

			m_bFireSingleShot = false;

            //Alundaio: Use fModeShotTime instead of fOneShotTime if current fire mode is 2-shot burst
            //Alundaio: Cycle down RPM after two shots; used for Abakan/AN-94
            if (GetCurrentFireMode() == 2 || (cycleDown == true && m_iShotNum <= 1))
                fShotTimeCounter = modeShotTime;
			else
				fShotTimeCounter = fOneShotTime;
            //Alundaio: END
			
			++m_iShotNum;
			
			OnShot();

			if (m_iShotNum>m_iShootEffectorStart)
				FireTrace(p1,d);
			else
				FireTrace(m_vStartPos, m_vStartDir);

			if (!m_bJamNotShot && CheckForMisfire())
			{
				StopShooting();
				return;
			}
		}
	
		if(m_iShotNum == m_iQueueSize)
			m_bStopedAfterQueueFired = true;

		UpdateSounds();
	}

	if(fShotTimeCounter<0)
	{

		StopShooting();
	}
	else
	{
		fShotTimeCounter -=	dt;
	}
}

void CWeaponMagazined::state_Misfire(float dt)
{
	OnEmptyClick();
	SwitchState(eIdle);
	
	bMisfire = true;

	UpdateSounds();
}

void CWeaponMagazined::switch2_LookMisfire()
{
    OnEmptyClick();
	PlayAnimLookMis();
    SetPending(true);

	if (smart_cast<CActor*>(this->H_Parent()) && (Level().CurrentViewEntity() == H_Parent()))
		HUD().GetUI()->AddInfoMessage("gun_jammed");
}

void CWeaponMagazined::switch2_LightMisfire()
{
    PlaySound("sndLightMis", get_LastFP());
    PlayAnimLightMis();
    SetPending(true);

	if (smart_cast<CActor*>(this->H_Parent()) && (Level().CurrentViewEntity() == H_Parent()))
		HUD().GetUI()->AddInfoMessage("gun_misfire");
}

void CWeaponMagazined::PlayAnimLightMis()
{
	if(IsZoomed())
        PlayHUDMotion("anm_shoot_lightmisfire_aim", false, this, GetState());
	else
		PlayHUDMotion("anm_shoot_lightmisfire", true, this, GetState());
}

void CWeaponMagazined::PlayAnimLookMis()
{
    auto bm = smart_cast<CWeaponBM16*>(this);

	if (!bm)
	{
		if(IsZoomed())
			PlayHUDMotion("anm_fakeshoot_aim_jammed", false, this, GetState());
		else
			PlayHUDMotion("anm_fakeshoot_jammed", true, this, GetState());
	}
	else
	{
		switch(m_magazine.size())
		{
            case 1:
			{
				if(IsZoomed())
					PlayHUDMotion("anm_fakeshoot_aim_jammed_1", false, this, GetState());
				else
					PlayHUDMotion("anm_fakeshoot_jammed_1", false, this, GetState());
			}break;
            case 2:
			{
				if(IsZoomed())
					PlayHUDMotion("anm_fakeshoot_aim_jammed_2", false, this, GetState());
				else
					PlayHUDMotion("anm_fakeshoot_jammed_2", false, this, GetState());
			}break;
		}
	}
}

void CWeaponMagazined::SetDefaults()
{
	CWeapon::SetDefaults();
}

void CWeaponMagazined::PlaySoundLowAmmo()
{
	if (!m_bUseLowAmmoSnd)
		return;

    if (iAmmoElapsed == 0)
        return;

	if (m_u32ACPlaySnd == 0)
        return;

    CWeaponKnife* knf = smart_cast<CWeaponKnife*>(m_pInventory->ActiveItem());

	if (knf)
		return;

    CWeaponBinoculars* binoc = smart_cast<CWeaponBinoculars*>(m_pInventory->ActiveItem());

	if (binoc)
		return;

	if (iAmmoElapsed <= m_u32ACPlaySnd)
		PlaySound("sndLowAmmo", get_LastSP());
}

void CWeaponMagazined::PlaySoundShot()
{
	if (ParentIsActor())
	{
		string128 sndName;
		strconcat(sizeof(sndName), sndName, m_sSndShotCurrent.c_str(), "Actor");
		if (m_layered_sounds.FindSoundItem(sndName, false))
		{
			m_layered_sounds.PlaySound(sndName, get_LastFP(), H_Root(), !!GetHUDmode(), false, (u8)-1);
			return;
		}
	}

	m_layered_sounds.PlaySound(m_sSndShotCurrent.c_str(), get_LastFP(), H_Root(), !!GetHUDmode(), false, (u8)-1);
}

void CWeaponMagazined::OnShot()
{
	// Sound
	PlaySoundShot();
    PlaySoundLowAmmo();

	// Camera	
	AddShotEffector();

	// Animation
	PlayAnimShoot();
	
	// Shell Drop
	Fvector vel;
	PHGetLinearVell(vel);
	if(!IsMisfire() && !m_bDisableShellParticles)
		OnShellDrop(get_LastSP(), vel);

	// Огонь из ствола
	StartFlameParticles();

	//дым из ствола
	ForceUpdateFireParticles();
	StartSmokeParticles(get_LastFP(), vel);
}

void CWeaponMagazined::OnShotJammed()
{
    // Sound
    PlaySound("sndJam", get_LastFP());

    // Animation
    PlayAnimShoot();
}

void CWeaponMagazined::OnEmptyClick()
{
	PlaySound("sndEmptyClick",get_LastFP());
}

void CWeaponMagazined::OnAnimationEnd(u32 state)
{
    switch (state) 
	{
		case eReload:
		{
		    if (IsMisfire() && !IsGrenadeLauncherMode())
		        Unmisfire();
		    else
				ReloadMagazine();

		    SwitchState(eIdle);
		    bSwitchAmmoType = false;
		}break; // End of reload animation
		case eHiding:
		{
		    SwitchState(eHidden);
		    bSwitchAmmoType = false;
		} break; // End of Hide
		case eHideDet:
		    SwitchState(eIdle);
			break;
		case eShowing:
		{
			SwitchState(eIdle);
			bSwitchAmmoType = false;
		} break;	// End of Show
		case eIdle:
			switch2_Idle();
			break;  // Keep showing idle
		case eEmpty:
			SwitchState(eIdle);
			break;
		case eLookMis:
			SwitchState(eIdle);
			break;
		case eUnLightMis:
			SwitchState(eIdle);
			break;
		case eShowingDet:
		{
		    auto det = smart_cast<CCustomDetector*>(m_pInventory->ItemFromSlot(DETECTOR_SLOT));
		    if (det) 
			{
				det->SwitchState(CCustomDetector::eShowing);
				SwitchState(eShowingEndDet);
				det->TurnDetectorInternal(true);
			}
		}break;
		case eShowingEndDet:
			SwitchState(eIdle);
			break;
		case eZoomStart:
			SwitchState(eIdle);
			break;
		case eZoomEnd:
			SwitchState(eIdle);
			break;
	}
	inherited::OnAnimationEnd(state);
}

void CWeaponMagazined::Unmisfire()
{
	bMisfire = false;
}

void CWeaponMagazined::switch2_Idle()
{
	SetPending(false);
	PlayAnimIdle();
}

#ifdef DEBUG
#include "ai\stalker\ai_stalker.h"
#include "object_handler_planner.h"
#endif
void CWeaponMagazined::switch2_Fire()
{
	CInventoryOwner* io	= smart_cast<CInventoryOwner*>(H_Parent());
	CInventoryItem* ii = smart_cast<CInventoryItem*>(this);
#ifdef DEBUG
	VERIFY2					(io,make_string("no inventory owner, item %s",*cName()));

	if (ii != io->inventory().ActiveItem())
		Msg					("! not an active item, item %s, owner %s, active item %s",*cName(),*H_Parent()->cName(),io->inventory().ActiveItem() ? *io->inventory().ActiveItem()->object().cName() : "no_active_item");

	if ( !(io && (ii == io->inventory().ActiveItem())) ) 
	{
		CAI_Stalker			*stalker = smart_cast<CAI_Stalker*>(H_Parent());
		if (stalker) {
			stalker->planner().show						();
			stalker->planner().show_current_world_state	();
			stalker->planner().show_target_world_state	();
		}
	}
#else
	if (!io)
		return;
#endif // DEBUG

	m_bStopedAfterQueueFired = false;
	m_bFireSingleShot = true;
	m_iShotNum = 0;

    if((OnClient() || Level().IsDemoPlay()) && !IsWorking())
		FireStart();

}

void CWeaponMagazined::PlayReloadSound()
{
    if (!IsMisfire() && iAmmoElapsed == 0 || bSwitchAmmoType && iAmmoElapsed == 0 && !IsMisfire())
		PlaySound("sndReloadEmpty", get_LastFP());
    else if (IsMisfire() && iAmmoElapsed == 0)
        PlaySound("sndReloadJammedLast", get_LastFP());
	else if(IsMisfire())
		PlaySound("sndReloadJammed", get_LastFP());
    else if (bSwitchAmmoType && iAmmoElapsed != 0)
        PlaySound("sndAmmoChange", get_LastFP());
	else
		PlaySound("sndReload", get_LastFP());
}

void CWeaponMagazined::switch2_Reload()
{
	CWeapon::FireEnd();

	PlayReloadSound();
	PlayAnimReload();
	SetPending(true);
}

void CWeaponMagazined::switch2_Hiding()
{
	OnZoomOut();
	CWeapon::FireEnd();
	
	PlaySound("sndHide", get_LastFP());

	PlayAnimHide();
	SetPending(true);
}

void CWeaponMagazined::switch2_Hidden()
{
	CWeapon::FireEnd();

	StopCurrentAnimWithoutCallback();

	signal_HideComplete();
	RemoveShotEffector();
}
void CWeaponMagazined::switch2_Showing()
{
	PlaySound("sndShow", get_LastFP());

	SetPending(true);
	PlayAnimShow();
}

bool CWeaponMagazined::Action(s32 cmd, u32 flags) 
{
	if(inherited::Action(cmd, flags)) return true;
	
	//если оружие чем-то занято, то ничего не делать
	if(IsPending()) return false;
	
	switch(cmd) 
	{
	case kWPN_RELOAD:
		{
			if(flags&CMD_START && !IsZoomed()) 
				if(iAmmoElapsed < iMagazineSize || (IsMisfire() && !IsGrenadeLauncherMode())) 
					Reload();
		} 
		return true;
	case kWPN_FIREMODE_PREV:
		{
			if(flags&CMD_START) 
			{
				OnPrevFireMode();
				return true;
			};
		}break;
	case kWPN_FIREMODE_NEXT:
		{
			if(flags&CMD_START) 
			{
				OnNextFireMode();
				return true;
			};
		}break;
	}
	return false;
}

bool CWeaponMagazined::CanAttach(PIItem pIItem)
{
	CScope*	pScope = smart_cast<CScope*>(pIItem);
	CSilencer* pSilencer = smart_cast<CSilencer*>(pIItem);
	CGrenadeLauncher* pGrenadeLauncher = smart_cast<CGrenadeLauncher*>(pIItem);

	if(pScope && m_eScopeStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonScope) == 0 && (m_sScopeName == pIItem->object().cNameSect()) )
       return true;
	else if(pSilencer && m_eSilencerStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonSilencer) == 0 && (m_sSilencerName == pIItem->object().cNameSect()) )
       return true;
	else if (pGrenadeLauncher && m_eGrenadeLauncherStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher) == 0 && (m_sGrenadeLauncherName  == pIItem->object().cNameSect()) )
		return true;
	else
		return inherited::CanAttach(pIItem);
}

bool CWeaponMagazined::CanDetach(const char* item_section_name)
{
	if(m_eScopeStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonScope) && (m_sScopeName == item_section_name))
       return true;
	else if(m_eSilencerStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonSilencer) && (m_sSilencerName == item_section_name))
       return true;
	else if(m_eGrenadeLauncherStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher) && (m_sGrenadeLauncherName == item_section_name))
       return true;
	else
		return inherited::CanDetach(item_section_name);
}

bool CWeaponMagazined::Attach(PIItem pIItem, bool b_send_event)
{
	bool result = false;

	CScope*	pScope = smart_cast<CScope*>(pIItem);
	CSilencer* pSilencer = smart_cast<CSilencer*>(pIItem);
	CGrenadeLauncher* pGrenadeLauncher = smart_cast<CGrenadeLauncher*>(pIItem);
	
	if(pScope && m_eScopeStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonScope) == 0 && (m_sScopeName == pIItem->object().cNameSect()))
	{
		m_flagsAddOnState |= CSE_ALifeItemWeapon::eWeaponAddonScope;
		result = true;
	}
	else if(pSilencer && m_eSilencerStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonSilencer) == 0 && (m_sSilencerName == pIItem->object().cNameSect()))
	{
        if (m_bRestGL_and_Sil && IsGrenadeLauncherAttached())
			Detach(GetGrenadeLauncherName().c_str(), true);

		m_flagsAddOnState |= CSE_ALifeItemWeapon::eWeaponAddonSilencer;
		result = true;
	}
	else if(pGrenadeLauncher && m_eGrenadeLauncherStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher) == 0 && (m_sGrenadeLauncherName == pIItem->object().cNameSect()))
	{
        if (m_bRestGL_and_Sil && IsSilencerAttached())
			Detach(GetSilencerName().c_str(), true);

		m_flagsAddOnState |= CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher;
		result = true;
	}

	if(result)
	{
		if (b_send_event && OnServer())
		{
			//уничтожить подсоединенную вещь из инвентаря
			pIItem->object().DestroyObject	();
		};

		UpdateAddonsVisibility();
		InitAddons();

		return true;
	}
	else
        return inherited::Attach(pIItem, b_send_event);
}

bool CWeaponMagazined::Detach(const char* item_section_name, bool b_spawn_item)
{
	if(m_eScopeStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonScope) && (m_sScopeName == item_section_name))
	{
		m_flagsAddOnState &= ~CSE_ALifeItemWeapon::eWeaponAddonScope;
		
		UpdateAddonsVisibility();
		InitAddons();

		return CInventoryItemObject::Detach(item_section_name, b_spawn_item);
	}
	else if(m_eSilencerStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonSilencer) && (m_sSilencerName == item_section_name))
	{
		m_flagsAddOnState &= ~CSE_ALifeItemWeapon::eWeaponAddonSilencer;

		UpdateAddonsVisibility();
		InitAddons();
		return CInventoryItemObject::Detach(item_section_name, b_spawn_item);
	}
	else if(m_eGrenadeLauncherStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher) && (m_sGrenadeLauncherName == item_section_name))
	{
		m_flagsAddOnState &= ~CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher;

		UpdateAddonsVisibility();
		InitAddons();
		return CInventoryItemObject::Detach(item_section_name, b_spawn_item);
	}
	else
		return inherited::Detach(item_section_name, b_spawn_item);;
}

void CWeaponMagazined::InitAddons()
{
	m_zoom_params.m_fIronSightZoomFactor = READ_IF_EXISTS(pSettings, r_float, cNameSect(), "ironsight_zoom_factor", 50.0f);
	if ( IsScopeAttached() )
	{
		shared_str scope_tex_name;
		if (m_eScopeStatus == ALife::eAddonAttachable)
		{
			VERIFY( *m_sScopeName );
			scope_tex_name = pSettings->r_string(*m_sScopeName, "scope_texture");
			m_zoom_params.m_fScopeZoomFactor = pSettings->r_float(*m_sScopeName, "scope_zoom_factor");
			m_fRTZoomFactor = m_zoom_params.m_fScopeZoomFactor;
		}
		else if(m_eScopeStatus == ALife::eAddonPermanent)
		{
			scope_tex_name = pSettings->r_string(cNameSect(), "scope_texture");
			m_zoom_params.m_fScopeZoomFactor = pSettings->r_float(cNameSect(), "scope_zoom_factor");
		}
		if (m_UIScope)
		{
			xr_delete(m_UIScope);
		}
		if (!g_dedicated_server)
		{
			m_UIScope = xr_new<CUIWindow>();
			createWpnScopeXML();
			CUIXmlInit::InitWindow(*pWpnScopeXml, scope_tex_name.c_str(), 0, m_UIScope);
		}
	}
	else
	{
		if (m_UIScope)
		{
			xr_delete(m_UIScope);
		}
		
		if ( IsZoomEnabled() )
		{
			m_zoom_params.m_fIronSightZoomFactor = pSettings->r_float(cNameSect(), "scope_zoom_factor");
		}
	}

	if (IsSilencerAttached() && SilencerAttachable())
	{		
		m_sFlameParticlesCurrent = m_sSilencerFlameParticles;
		m_sSmokeParticlesCurrent = m_sSilencerSmokeParticles;
		m_sSndShotCurrent = "sndSilencerShot";

		if (m_bUseSilHud && IsSilencerAttached())
			hud_sect = pSettings->r_string(cNameSect(), "hud_silencer");

		//подсветка от выстрела
		LoadLights(*cNameSect(), "silencer_");
		ApplySilencerKoeffs();
	}
	else
	{
		m_sFlameParticlesCurrent = m_sFlameParticles;
		m_sSmokeParticlesCurrent = m_sSmokeParticles;
		m_sSndShotCurrent = "sndShot";

		if (m_bUseSilHud && !IsSilencerAttached())
			hud_sect = pSettings->r_string(cNameSect(), "hud");

		//подсветка от выстрела
		LoadLights(*cNameSect(), "");
		ResetSilencerKoeffs();
	}

	inherited::InitAddons();
}

void CWeaponMagazined::LoadSilencerKoeffs()
{
	if (m_eSilencerStatus == ALife::eAddonAttachable)
	{
		LPCSTR sect = m_sSilencerName.c_str();
		m_silencer_koef.hit_power = READ_IF_EXISTS(pSettings, r_float, sect, "bullet_hit_power_k", 1.0f);
		m_silencer_koef.hit_impulse	= READ_IF_EXISTS(pSettings, r_float, sect, "bullet_hit_impulse_k", 1.0f);
		m_silencer_koef.bullet_speed = READ_IF_EXISTS(pSettings, r_float, sect, "bullet_speed_k", 1.0f);
		m_silencer_koef.fire_dispersion	= READ_IF_EXISTS(pSettings, r_float, sect, "fire_dispersion_base_k", 1.0f);
		m_silencer_koef.cam_dispersion = READ_IF_EXISTS(pSettings, r_float, sect, "cam_dispersion_k", 1.0f);
		m_silencer_koef.cam_disper_inc = READ_IF_EXISTS(pSettings, r_float, sect, "cam_dispersion_inc_k", 1.0f);
	}

	clamp(m_silencer_koef.hit_power, 0.0f, 1.0f);
	clamp(m_silencer_koef.hit_impulse, 0.0f, 1.0f);
	clamp(m_silencer_koef.bullet_speed,	0.0f, 1.0f);
	clamp(m_silencer_koef.fire_dispersion, 0.0f, 3.0f);
	clamp(m_silencer_koef.cam_dispersion, 0.0f, 1.0f);
	clamp(m_silencer_koef.cam_disper_inc, 0.0f, 1.0f);
}

void CWeaponMagazined::ApplySilencerKoeffs()
{
	cur_silencer_koef = m_silencer_koef;
}

void CWeaponMagazined::ResetSilencerKoeffs()
{
	cur_silencer_koef.Reset();
}

void CWeaponMagazined::PlayAnimShow()
{
	VERIFY(GetState()==eShowing);

	if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_show_empty", false, this, GetState());
	else if(IsMisfire())
		PlayHUDMotion("anm_show_jammed", false, this, GetState());
	else
		PlayHUDMotion("anm_show", false, this, GetState());
}

void CWeaponMagazined::PlayAnimShowDet()
{
	VERIFY(GetState()==eShowingDet);

	if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_prepare_detector_empty", false, this, GetState());
	else if(IsMisfire())
		PlayHUDMotion("anm_prepare_detector_jammed", false, this, GetState());
	else
		PlayHUDMotion("anm_prepare_detector", false, this, GetState());
}

void CWeaponMagazined::PlayAnimShowEndDet()
{
    VERIFY(GetState() == eShowingEndDet);

    if (iAmmoElapsed == 0)
        PlayHUDMotion("anm_draw_detector_empty", false, this, GetState());
    else if (IsMisfire())
        PlayHUDMotion("anm_draw_detector_jammed", false, this, GetState());
    else
        PlayHUDMotion("anm_draw_detector", false, this, GetState());
}

void CWeaponMagazined::PlayAnimHide()
{
	VERIFY(GetState()==eHiding);

	if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_hide_empty", true, this, GetState());
	else if(IsMisfire())
		PlayHUDMotion("anm_hide_jammed", true, this, GetState());
	else
		PlayHUDMotion("anm_hide", true, this, GetState());
}

void CWeaponMagazined::PlayAnimHideDet()
{
    VERIFY(GetState()==eHideDet);

    if (!IsMisfire() && iAmmoElapsed == 0)
        PlayHUDMotion("anm_finish_detector_empty", true, this, GetState());
    else if (IsMisfire())
        PlayHUDMotion("anm_finish_detector_jammed", true, this, GetState());
	else
        PlayHUDMotion("anm_finish_detector", true, this, GetState());
}

void CWeaponMagazined::PlayAnimReload()
{
	VERIFY(GetState()==eReload);

	if(!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_reload_empty", true, this, GetState());
	else if(IsMisfire() && iAmmoElapsed != 0)
		PlayHUDMotion("anm_reload_jammed", true, this, GetState());
	else if(IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_reload_jammed_last", true, this, GetState());
	else if(bSwitchAmmoType)
		PlayHUDMotion("anm_reload_ammochange", true, this, GetState());
	else if(bSwitchAmmoType && iAmmoElapsed == 0)
		PlayHUDMotion("anm_reload_empty_ammochange", true, this, GetState());
	else
		PlayHUDMotion("anm_reload", true, this, GetState());
}

void CWeaponMagazined::PlayAnimAimStart()
{
    VERIFY(GetState() == eZoomStart);

	auto bm = smart_cast<CWeaponBM16*>(this);

	if(!bm)
	{
		if (!IsMisfire() && iAmmoElapsed == 0)
			PlayHUDMotion("anm_idle_aim_start_empty", true, this, GetState());
		else if (IsMisfire())
			PlayHUDMotion("anm_idle_aim_start_jammed", true, this, GetState());
		else
			PlayHUDMotion("anm_idle_aim_start", true, this, GetState());
	}
	else
	{
		switch (m_magazine.size())
		{
            case 0:
			{
				if(IsMisfire())
					PlayHUDMotion("anm_idle_aim_start_jammed_0", true, this, GetState());
				else
					PlayHUDMotion("anm_idle_aim_start_0", true, this, GetState());
			}break;
            case 1:
			{
				if(IsMisfire())
					PlayHUDMotion("anm_idle_aim_start_jammed_1", true, this, GetState());
				else
					PlayHUDMotion("anm_idle_aim_start_1", true, this, GetState());
			}break;
            case 2:
			{
				if(IsMisfire())
					PlayHUDMotion("anm_idle_aim_start_jammed_2", true, this, GetState());
				else
					PlayHUDMotion("anm_idle_aim_start_2", true, this, GetState());
			}break;
		}
	}
}

void CWeaponMagazined::PlayAnimIdleMoving()
{
    if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_idle_moving_empty", true, nullptr, GetState());
	else if(IsMisfire())
		PlayHUDMotion("anm_idle_moving_jammed", true, nullptr, GetState());
	else
		PlayHUDMotion("anm_idle_moving", true, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleSprint()
{
    if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_idle_sprint_empty", true, nullptr, GetState());
	else if(IsMisfire())
		PlayHUDMotion("anm_idle_sprint_jammed", true, nullptr, GetState());
	else
		PlayHUDMotion("anm_idle_sprint", true, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleMovingCrouch()
{
    if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_idle_moving_crouch_empty", true, nullptr, GetState());
	else if (IsMisfire())
		PlayHUDMotion("anm_idle_moving_crouch_jammed", true, nullptr, GetState());
	else
		PlayHUDMotion("anm_idle_moving_crouch", true, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleMovingCrouchSlow()
{
    if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_idle_moving_crouch_slow_empty", true, nullptr, GetState());
    else if (IsMisfire())
		PlayHUDMotion("anm_idle_moving_crouch_slow_jammed", true, nullptr, GetState());
	else
		PlayHUDMotion("anm_idle_moving_crouch_slow", true, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleMovingSlow()
{
    if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_idle_moving_slow_empty", true, nullptr, GetState());
    else if (IsMisfire())
		PlayHUDMotion("anm_idle_moving_slow_jammed", true, nullptr, GetState());
	else
		PlayHUDMotion("anm_idle_moving_slow", true, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimAimEnd()
{
    VERIFY(GetState() == eZoomEnd);

	auto bm = smart_cast<CWeaponBM16*>(this);

	if(!bm)
	{
		if (!IsMisfire() && iAmmoElapsed == 0)
			PlayHUDMotion("anm_idle_aim_end_empty", true, this, GetState());
		else if (IsMisfire())
			PlayHUDMotion("anm_idle_aim_end_jammed", true, this, GetState());
		else
			PlayHUDMotion("anm_idle_aim_end", true, this, GetState());
	}
	else
	{
		switch (m_magazine.size())
		{
            case 0:
			{
				if(IsMisfire())
					PlayHUDMotion("anm_idle_aim_end_jammed_0", true, this, GetState());
				else
					PlayHUDMotion("anm_idle_aim_end_0", true, this, GetState());
			}break;
            case 1:
			{
				if(IsMisfire())
					PlayHUDMotion("anm_idle_aim_end_jammed_1", true, this, GetState());
				else
					PlayHUDMotion("anm_idle_aim_end_1", true, this, GetState());
			}break;
            case 2:
			{
				if(IsMisfire())
					PlayHUDMotion("anm_idle_aim_end_jammed_2", true, this, GetState());
				else
					PlayHUDMotion("anm_idle_aim_end_2", true, this, GetState());
			}break;
		}
	}
}

void CWeaponMagazined::EmptyMove()
{
    VERIFY(GetState() == eEmpty);

    OnEmptyClick(); //звук

    if (IsZoomed())
        PlayHUDMotion("anm_fakeshoot_aim_empty", false, this, GetState());
    else
        PlayHUDMotion("anm_fakeshoot_empty", false, this, GetState());
}

const char* CWeaponMagazined::GetAnimAimName()
{
	auto pActor = smart_cast<const CActor*>(H_Parent());
    u32 state = pActor->get_state();
	if (pActor)
	{
        if (state & mcAnyMove)
		{
			if (IsScopeAttached())
			{
				strcpy_s(guns_aim_anm, "anm_idle_aim_scope_moving");
				return guns_aim_anm;
			}
			else
				return xr_strconcat(guns_aim_anm, "anm_idle_aim_moving", (state & mcFwd) ? "_forward" : ((state & mcBack) ? "_back" : ""), (state & mcLStrafe) ? "_left" : ((state & mcRStrafe) ? "_right" : ""), IsMisfire() ? "_jammed" : (!IsMisfire() && iAmmoElapsed == 0 ? "_empty" : ""));
		}
	}
	return nullptr;
}

void CWeaponMagazined::PlayAnimAim()
{
	if (const char* guns_aim_anm = GetAnimAimName())
	{
		if (isHUDAnimationExist(guns_aim_anm))
		{
			PlayHUDMotionNew(guns_aim_anm, true, GetState());
			return;
		}
	}

	if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_idle_aim_empty", true, nullptr, GetState());
	else if (IsMisfire())
		PlayHUDMotion("anm_idle_aim_jammed", true, nullptr, GetState());
	else
		PlayHUDMotion("anm_idle_aim", true, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdle()
{
    VERIFY(GetState() == eIdle);

    if (TryPlayAnimIdle())
        return;

    if (IsZoomed())
        PlayAnimAim();
    else if (!IsMisfire() && iAmmoElapsed == 0)
        PlayHUDMotion("anm_idle_empty", true, nullptr, GetState());
    else if (IsMisfire())
        PlayHUDMotion("anm_idle_jammed", true, nullptr, GetState());
    else
        PlayHUDMotion("anm_idle", true, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimShoot()
{
	VERIFY(GetState()==eFire);
	
    string_path guns_shoot_anm{};
    xr_strconcat(guns_shoot_anm, "anm_shoot", (IsZoomed() && !IsRotatingToZoom()) ? (IsScopeAttached() ?  "_aim_scope" : "_aim")  : "", IsMisfire() ? "_jammed" : ( !IsMisfire() && iAmmoElapsed == 1 ? "_last" : ""), IsSilencerAttached() ? "_sil" : "");

    PlayHUDMotionNew(guns_shoot_anm, false, GetState());
}

void CWeaponMagazined::OnZoomIn()
{
	inherited::OnZoomIn();

	if(GetState() == eIdle)
		PlayAnimIdle();


	CActor* pActor = smart_cast<CActor*>(H_Parent());
	if(pActor)
	{
		CEffectorZoomInertion* S = smart_cast<CEffectorZoomInertion*>	(pActor->Cameras().GetCamEffector(eCEZoom));
		if (!S)	
		{
			S = (CEffectorZoomInertion*)pActor->Cameras().AddCamEffector(xr_new<CEffectorZoomInertion>());
			S->Init(this);
		};
		S->SetRndSeed(pActor->GetZoomRndSeed());
		R_ASSERT(S);
	}
}
void CWeaponMagazined::OnZoomOut()
{
	if(!IsZoomed())	 
		return;

	inherited::OnZoomOut();

	if(GetState()==eIdle)
		PlayAnimIdle();

	CActor* pActor = smart_cast<CActor*>(H_Parent());

	if(pActor)
		pActor->Cameras().RemoveCamEffector	(eCEZoom);

}

//переключение режимов стрельбы одиночными и очередями
bool CWeaponMagazined::SwitchMode()
{
	if(eIdle != GetState() || IsPending()) return false;

	if(SingleShotMode())
		m_iQueueSize = WEAPON_ININITE_QUEUE;
	else
		m_iQueueSize = 1;
	
	PlaySound("sndEmptyClick", get_LastFP());

	return true;
}

void CWeaponMagazined::OnNextFireMode()
{
	if (!m_bHasDifferentFireModes) return;
	if (GetState() != eIdle) return;
	m_iCurFireMode = (m_iCurFireMode+1+m_aFireModes.size()) % m_aFireModes.size();
	SetQueueSize(GetCurrentFireMode());
};

void CWeaponMagazined::OnPrevFireMode()
{
	if (!m_bHasDifferentFireModes) return;
	if (GetState() != eIdle) return;
	m_iCurFireMode = (m_iCurFireMode-1+m_aFireModes.size()) % m_aFireModes.size();
	SetQueueSize(GetCurrentFireMode());	
};

void CWeaponMagazined::OnH_A_Chield()
{
	if (m_bHasDifferentFireModes)
	{
		CActor	*actor = smart_cast<CActor*>(H_Parent());
		if (!actor) SetQueueSize(-1);
		else SetQueueSize(GetCurrentFireMode());
	};	
	inherited::OnH_A_Chield();
};

void CWeaponMagazined::SetQueueSize(int size)  
{
	m_iQueueSize = size; 
};

void CWeaponMagazined::save(NET_Packet &output_packet)
{
	inherited::save	(output_packet);
	save_data(m_iQueueSize, output_packet);
	save_data(m_iShotNum, output_packet);
	save_data(m_iCurFireMode, output_packet);
}

void CWeaponMagazined::load(IReader &input_packet)
{
	inherited::load	(input_packet);
	load_data(m_iQueueSize, input_packet);SetQueueSize(m_iQueueSize);
	load_data(m_iShotNum, input_packet);
	load_data(m_iCurFireMode, input_packet);
}

void CWeaponMagazined::net_Export(NET_Packet& P)
{
	inherited::net_Export (P);

	P.w_u8(u8(m_iCurFireMode&0x00ff));
}

void CWeaponMagazined::net_Import(NET_Packet& P)
{
	inherited::net_Import (P);

	m_iCurFireMode = P.r_u8();
	SetQueueSize(GetCurrentFireMode());
}

#include "string_table.h"
void CWeaponMagazined::GetBriefInfo(xr_string& str_name, xr_string& icon_sect_name, xr_string& str_count, string16& fire_mode )
{
	int	AE = GetAmmoElapsed();
	int	AC = 0;
	if ( IsGameTypeSingle())
	{
		AC = GetCurrentTypeAmmoTotal();
	}
	else
	{
		AC = GetSuitableAmmoTotal();//mp = all type
	}
	
	if(AE == 0 || 0 == m_magazine.size())
		icon_sect_name = *m_ammoTypes[m_ammoType];
	else
		icon_sect_name = *m_ammoTypes[m_magazine.back().m_LocalAmmoType];


	string256 sItemName;
	strcpy_s(sItemName, *CStringTable().translate(pSettings->r_string(icon_sect_name.c_str(), "inv_name_short")));

	strcpy_s(fire_mode, sizeof(fire_mode), "");
	if (HasFireModes())
	{
		if (m_iQueueSize == -1)
			strcpy_s(fire_mode, "A");
		else
			sprintf_s(fire_mode, "%d", m_iQueueSize);
	}

	str_name = sItemName;

	{
		if (!unlimited_ammo())
			sprintf_s(sItemName, "%d/%d",AE,AC - AE);
		else
			sprintf_s(sItemName, "%d/--",AE);

		str_count = sItemName;
	}
}

bool CWeaponMagazined::install_upgrade_impl(LPCSTR section, bool test)
{
	bool result = inherited::install_upgrade_impl( section, test );
	
	LPCSTR str;
	// fire_modes = 1, 2, -1
	bool result2 = process_if_exists_set(section, "fire_modes", &CInifile::r_string, str, test);
	if (result2 && !test)
	{
		int ModesCount = _GetItemCount(str);
		m_aFireModes.clear();
		for (int i = 0; i < ModesCount; ++i)
		{
			string16 sItem;
			_GetItem(str, i, sItem);
			m_aFireModes.push_back((s8)atoi(sItem));
		}
		m_iCurFireMode = ModesCount - 1;
	}
	result |= result2;

	result |= process_if_exists(section, "dispersion_start", &CInifile::r_s32, m_iShootEffectorStart, test);

	// sounds (name of the sound, volume (0.0 - 1.0), delay (sec))
	result2 = process_if_exists_set(section, "snd_draw", &CInifile::r_string, str, test);
	if (result2 && !test)
	{
		m_sounds.LoadSound(section, "snd_draw", "sndShow", false, m_eSoundShow);
	}
	result |= result2;

	result2 = process_if_exists_set(section, "snd_holster", &CInifile::r_string, str, test);
	if (result2 && !test)
	{
		m_sounds.LoadSound(section, "snd_holster", "sndHide", false, m_eSoundHide);
	}
	result |= result2;

	result2 = process_if_exists_set(section, "snd_shoot", &CInifile::r_string, str, test);
	if ( result2 && !test )
	{
		m_layered_sounds.LoadSound(section, "snd_shoot", "sndShot", false, m_eSoundShot);
	}
	result |= result2;

	result2 = process_if_exists_set(section, "snd_empty", &CInifile::r_string, str, test);
	if ( result2 && !test )
	{
		m_sounds.LoadSound( section, "snd_empty", "sndEmptyClick", false, m_eSoundEmptyClick);
	}
	result |= result2;

	result2 = process_if_exists_set(section, "snd_reload", &CInifile::r_string, str, test);
	if ( result2 && !test )
	{
		m_sounds.LoadSound(section, "snd_reload", "sndReload", true, m_eSoundReload);
	}
	result |= result2;

	if (m_eSilencerStatus == ALife::eAddonAttachable)
	{
		result |= process_if_exists_set(section, "silencer_flame_particles", &CInifile::r_string, m_sSilencerFlameParticles, test);
		result |= process_if_exists_set(section, "silencer_smoke_particles", &CInifile::r_string, m_sSilencerSmokeParticles, test);

		result2 = process_if_exists_set(section, "snd_silncer_shot", &CInifile::r_string, str, test);
		if ( result2 && !test )
		{
			m_layered_sounds.LoadSound(section, "snd_silncer_shot", "sndSilencerShot", false, m_eSoundShot);
		}
		result |= result2;
	}

	// fov for zoom mode
	result |= process_if_exists(section, "ironsight_zoom_factor", &CInifile::r_float, m_zoom_params.m_fIronSightZoomFactor, test);

	if(IsScopeAttached())
	{
		result |= process_if_exists(section, "scope_zoom_factor", &CInifile::r_float, m_zoom_params.m_fScopeZoomFactor, test);
	}
	else
	{
		if(IsZoomEnabled())
		{
			result |= process_if_exists(section, "scope_zoom_factor", &CInifile::r_float, m_zoom_params.m_fIronSightZoomFactor, test);
		}
	}

	return result;
}