#include "pch_script.h"
#include "hudmanager.h"
#include "WeaponMagazined.h"
#include "Entity.h"
#include "Actor.h"
#include "ParticlesObject.h"
#include "Scope.h"
#include "Silencer.h"
#include "GrenadeLauncher.h"
#include "Handler.h"
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
#include "player_hud.h"

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

	m_sFireModeMask_1 = nullptr;
	m_sFireModeMask_3 = nullptr;
	m_sFireModeMask_a = nullptr;
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
		
	auto LoadBoneNames = [](shared_str section, LPCTSTR line, RStringVec& list)
	{
		list.clear();
		if(pSettings->line_exist(section, line))
		{
			pcstr lineStr = pSettings->r_string(section, line);
			for (int j = 0, cnt = _GetItemCount(lineStr); j < cnt; ++j)
			{
				string128 bone_name;
				_GetItem(lineStr, j, bone_name);
				list.push_back(bone_name);
			}
			return true;
		}
		return false;
	};

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

		m_sounds.LoadSound(section, "snd_changefiremode", "sndFireMode", false, m_eSoundShow);

		if (pSettings->line_exist(hud_sect, "mask_firemode_1"))
			m_sFireModeMask_1 = pSettings->r_string(hud_sect, "mask_firemode_1");

		if (pSettings->line_exist(hud_sect, "mask_firemode_3"))
			m_sFireModeMask_3 = pSettings->r_string(hud_sect, "mask_firemode_3");

		if (pSettings->line_exist(hud_sect, "mask_firemode_a"))
			m_sFireModeMask_a = pSettings->r_string(hud_sect, "mask_firemode_a");

		if (pSettings->line_exist(hud_sect, "firemode_bones_total"))
		{
			LoadBoneNames(hud_sect, "firemode_bones_total", m_sFireModeBonesTotal);
			LoadBoneNames(hud_sect, "firemode_bones_1", m_sFireModeBone_1);
			LoadBoneNames(hud_sect, "firemode_bones_3", m_sFireModeBone_3);
			LoadBoneNames(hud_sect, "firemode_bones_a", m_sFireModeBone_a);
		}
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

				attachable_hud_item* i1 = g_player_hud->attached_item(1);
				if (i1 && HudItemData())
				{
					auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
					if (m_bDisableLMDet && det && det->GetState() == CCustomDetector::eShowing)
						return;
				}

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
	if(dynamic_cast<CActor*>(this->H_Parent()) && (Level().CurrentViewEntity()==H_Parent()))
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
    CWeaponBinoculars* binoc = dynamic_cast<CWeaponBinoculars*>(m_pInventory->ActiveItem());
	if(m_pInventory && !binoc) 
	{
		if(IsMisfire())
		{
			SetPending(true);
			SwitchState(eReload); 
			return true;
		}

		if(IsGameTypeSingle() && ParentIsActor())
		{
			int	AC = GetSuitableAmmoTotal();
			Actor()->callback(GameObject::eWeaponNoAmmoAvailable)(lua_game_object(), AC);
		}
		if (m_ammoType < m_ammoTypes.size())
			m_pAmmo = dynamic_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[m_ammoType]));
		else
			m_pAmmo = nullptr;

		if(m_pAmmo || unlimited_ammo())  
		{
			SetPending(true);
			SwitchState(eReload); 
			return true;
		} 
		else for(u32 i = 0; i < m_ammoTypes.size(); ++i) 
		{
			m_pAmmo = dynamic_cast<CWeaponAmmo*>(m_pInventory->GetAny( *m_ammoTypes[i] ));
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
	if (dynamic_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[m_ammoType])))
		return	(true);
	else
		for(u32 i = 0; i < m_ammoTypes.size(); ++i)
			if (dynamic_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[i])))
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
		CWeaponAmmo *l_pA = dynamic_cast<CWeaponAmmo*>(m_pInventory->GetAny(l_it->first));
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
		m_pAmmo = dynamic_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[m_ammoType]));
		
		if(!m_pAmmo && !m_bLockType) 
		{
			for(u32 i = 0; i < m_ammoTypes.size(); ++i) 
			{
				//проверить патроны всех подходящих типов
				m_pAmmo = dynamic_cast<CWeaponAmmo*>(m_pInventory->GetAny(*m_ammoTypes[i]));
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
        switch (S)
		{
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
        }break;
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
		case eSwitchMode:
		{
			switch2_FireMode();
		}break;
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
		case eSwitch:
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
		case eSwitchMode:
			UpdateAddonsVisibility();
		break;
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
		if (dynamic_cast<CMPPlayersBag*>(H_Parent()) != nullptr)
		{
			Msg("! WARNING: state_Fire of object [%d][%s] while parent is CMPPlayerBag...", ID(), cNameSect().c_str());
			return;
		}

		CInventoryOwner* io	= dynamic_cast<CInventoryOwner*>(H_Parent());
		if(nullptr == io->inventory().ActiveItem())
		{
				Log("current_state", GetState());
				Log("next_state", GetNextState());
				Log("item_sect", cNameSect().c_str());
				Log("H_Parent", H_Parent()->cNameSect().c_str());
		}

		CEntity* E = dynamic_cast<CEntity*>(H_Parent());
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

	if (dynamic_cast<CActor*>(this->H_Parent()) && (Level().CurrentViewEntity() == H_Parent()))
		HUD().GetUI()->AddInfoMessage("gun_jammed");

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && det->GetState() == CCustomDetector::eIdle)
			det->SwitchState(CCustomDetector::eLookMisDet);
	}
}

void CWeaponMagazined::switch2_LightMisfire()
{
    PlaySound("sndLightMis", get_LastFP());
    PlayAnimLightMis();
    SetPending(true);

	if (dynamic_cast<CActor*>(this->H_Parent()) && (Level().CurrentViewEntity() == H_Parent()))
		HUD().GetUI()->AddInfoMessage("gun_misfire");

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (!m_bDisableLMDet && det && det->GetState() == CCustomDetector::eIdle)
			det->SwitchState(CCustomDetector::eUnLightMisDet);
	}
}

void CWeaponMagazined::PlayAnimLightMis()
{
	std::string anm_name = "anm_shoot_lightmisfire";
	auto firemode = GetQueueSize();

	if (IsZoomed())
		anm_name += "_aim";
	else
		anm_name += "";

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

void CWeaponMagazined::PlayAnimLookMis()
{
    auto bm = dynamic_cast<CWeaponBM16*>(this);

	if (!bm)
	{
		std::string anm_name = "anm_fakeshoot";
		auto firemode = GetQueueSize();

		if(IsZoomed())
			anm_name += "_aim";
		else
			anm_name += "";

		if (firemode == -1 && m_sFireModeMask_a != nullptr)
			anm_name += m_sFireModeMask_a.c_str();
		else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
			anm_name += m_sFireModeMask_1.c_str();
		else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
			anm_name += m_sFireModeMask_3.c_str();
		else
			anm_name += "";

		anm_name += "_jammed";

		PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());

		attachable_hud_item* i1 = g_player_hud->attached_item(1);
		if (i1 && HudItemData())
		{
			auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
			if (det && det->GetState() == CCustomDetector::eIdle)
					det->SwitchState(CCustomDetector::eLookMisDet);
		}
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

	if (m_iACPlaySnd == 0)
        return;

    CWeaponKnife* knf = dynamic_cast<CWeaponKnife*>(m_pInventory->ActiveItem());

	if (knf)
		return;

    CWeaponBinoculars* binoc = dynamic_cast<CWeaponBinoculars*>(m_pInventory->ActiveItem());

	if (binoc)
		return;

	if (iAmmoElapsed <= m_iACPlaySnd)
		PlaySound("sndLowAmmo", get_LastSP());
}

void CWeaponMagazined::ShotSoundSelector()
{
	if(!IsSilencerAttached())
		m_sSndShotCurrent = "sndShot";
	else
		m_sSndShotCurrent = "sndSilencerShot";
}

void CWeaponMagazined::OnShot()
{
	// Sound
	ShotSoundSelector();
	m_layered_sounds.PlaySound(m_sSndShotCurrent.c_str(), get_LastFP(), H_Root(), !!GetHUDmode(), false, (u8)-1);
    PlaySoundLowAmmo();

	// Camera	
	AddShotEffector();

	// Animation
	PlayAnimShoot();
	
	// Shell Drop
	Fvector vel;
	PHGetLinearVell(vel);
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
		}break; // End of reload animation
		case eHiding:
		    SwitchState(eHidden);
		break; // End of Hide
		case eIdle:
			switch2_Idle();
		break;  // Keep showing idle
		case eShowingDet:
		{
		    auto det = dynamic_cast<CCustomDetector*>(m_pInventory->ItemFromSlot(DETECTOR_SLOT));
		    if (det) 
			{
				det->SwitchState(CCustomDetector::eShowing);
				det->TurnDetectorInternal(true);
				SwitchState(eShowingEndDet);
			}
		}break;
		case eEmpty:
		case eLookMis:
		case eUnLightMis:
		case eShowingEndDet:
		case eZoomStart:
		case eZoomEnd:
		case eSwitchMode:
		case eSwitch:
		case eHideDet:
		case eShowing:
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
	CInventoryOwner* io	= dynamic_cast<CInventoryOwner*>(H_Parent());
	CInventoryItem* ii = dynamic_cast<CInventoryItem*>(this);
#ifdef DEBUG
	VERIFY2					(io,make_string("no inventory owner, item %s",*cName()));

	if (ii != io->inventory().ActiveItem())
		Msg					("! not an active item, item %s, owner %s, active item %s",*cName(),*H_Parent()->cName(),io->inventory().ActiveItem() ? *io->inventory().ActiveItem()->object().cName() : "no_active_item");

	if ( !(io && (ii == io->inventory().ActiveItem())) ) 
	{
		CAI_Stalker			*stalker = dynamic_cast<CAI_Stalker*>(H_Parent());
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
    if (!IsMisfire() && iAmmoElapsed == 0 || IsChangeAmmoType() && iAmmoElapsed == 0 && !IsMisfire())
		PlaySound("sndReloadEmpty", get_LastFP());
    else if (IsMisfire() && iAmmoElapsed == 0)
        PlaySound("sndReloadJammedLast", get_LastFP());
	else if(IsMisfire())
		PlaySound("sndReloadJammed", get_LastFP());
    else if (IsChangeAmmoType() && iAmmoElapsed != 0)
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

void CWeaponMagazined::switch2_FireMode()
{
	SetPending(TRUE);
	PlayAnimFireMode();
	PlaySound("sndFireMode", get_LastFP());

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && det->GetState() == CCustomDetector::eIdle && m_magazine.size() != 0)
			det->SwitchState(CCustomDetector::eSwitchModeDet);
	}
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
			if(flags&CMD_START && !IsZoomed() && GetState() == eIdle) 
			{
				attachable_hud_item* i1 = g_player_hud->attached_item(1);
				if (i1 && HudItemData())
				{
					auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
					if (det && (det->GetState() != CCustomDetector::eIdle || det->m_bNeedActivation))
						return false;
				}

				if (ParentIsActor() && Actor()->IsSafemode())
				{
					Actor()->SetSafemodeStatus(false);
					return false;
				}

				if(iAmmoElapsed < iMagazineSize || (IsMisfire() && !IsGrenadeLauncherMode())) 
					Reload();
			}
		} 
		return true;
	case kWPN_FIREMODE_PREV:
		{
			if (ParentIsActor() && Actor()->IsSafemode())
			{
				Actor()->SetSafemodeStatus(false);
				return false;
			}

			if(flags&CMD_START) 
			{
				OnPrevFireMode();
				return true;
			};
		}break;
	case kWPN_FIREMODE_NEXT:
		{
			if (ParentIsActor() && Actor()->IsSafemode())
			{
				Actor()->SetSafemodeStatus(false);
				return false;
			}

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
	CScope*	pScope = dynamic_cast<CScope*>(pIItem);
	CSilencer* pSilencer = dynamic_cast<CSilencer*>(pIItem);
	CGrenadeLauncher* pGrenadeLauncher = dynamic_cast<CGrenadeLauncher*>(pIItem);
	CHandler* pHandler = dynamic_cast<CHandler*>(pIItem);

	if(pScope && m_eScopeStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonScope) == 0 && (m_sScopeName == pIItem->object().cNameSect()) )
       return true;
	else if(pSilencer && m_eSilencerStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonSilencer) == 0 && (m_sSilencerName == pIItem->object().cNameSect()) )
       return true;
	else if (pGrenadeLauncher && m_eGrenadeLauncherStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher) == 0 && (m_sGrenadeLauncherName  == pIItem->object().cNameSect()) )
		return true;
	else if (pHandler && m_eHandlerStatus == ALife::eAddonAttachable && (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonHandler) == 0 && (m_sHandlerName  == pIItem->object().cNameSect()))
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
	else if (m_eHandlerStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonHandler) && (m_sHandlerName == item_section_name))
		return true;
	else
		return inherited::CanDetach(item_section_name);
}

bool CWeaponMagazined::Attach(PIItem pIItem, bool b_send_event)
{
	bool result = false;

	CScope*	pScope = dynamic_cast<CScope*>(pIItem);
	CSilencer* pSilencer = dynamic_cast<CSilencer*>(pIItem);
	CGrenadeLauncher* pGrenadeLauncher = dynamic_cast<CGrenadeLauncher*>(pIItem);
	CHandler* pHandler = dynamic_cast<CHandler*>(pIItem);
	
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

        if (IsHandlerAttached())
			Detach(GetHandlerName().c_str(), true);

		m_flagsAddOnState |= CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher;
		result = true;
	}
	else if (pHandler && m_eHandlerStatus == ALife::eAddonAttachable && (m_flagsAddOnState & CSE_ALifeItemWeapon::eWeaponAddonHandler) == 0 && (m_sHandlerName == pIItem->object().cNameSect()))
	{
        if (IsGrenadeLauncherAttached())
			Detach(GetGrenadeLauncherName().c_str(), true);

		m_flagsAddOnState |= CSE_ALifeItemWeapon::eWeaponAddonHandler;
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
	else if(m_eHandlerStatus == ALife::eAddonAttachable && 0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonHandler) && (m_sHandlerName == item_section_name))
	{
		m_flagsAddOnState &= ~CSE_ALifeItemWeapon::eWeaponAddonHandler;

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

		//подсветка от выстрела
		LoadLights(*cNameSect(), "silencer_");
		ApplySilencerKoeffs();
	}
	else
	{
		m_sFlameParticlesCurrent = m_sFlameParticles;
		m_sSmokeParticlesCurrent = m_sSmokeParticles;

		//подсветка от выстрела
		LoadLights(*cNameSect(), "");
		ResetSilencerKoeffs();
	}

	HudSelector();
	inherited::InitAddons();
}

void CWeaponMagazined::HudSelector()
{
	if (m_bUseSilHud && SilencerAttachable() && IsSilencerAttached())
		hud_sect = pSettings->r_string(cNameSect(), "hud_silencer");
	else
		hud_sect = hud_sect_cache;
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
	VERIFY(GetState() == eShowing);

	if (!IsMisfire() && iAmmoElapsed == 0)
		PlayHUDMotion("anm_show_empty", false, this, GetState());
	else if(IsMisfire())
		PlayHUDMotion("anm_show_jammed", false, this, GetState());
	else
		PlayHUDMotion("anm_show", false, this, GetState());

	std::string anm_name = "anm_show";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), FALSE, this, GetState());
}

void CWeaponMagazined::PlayAnimFireMode()
{
	VERIFY(GetState() == eSwitchMode);

	std::string anm_name = "anm_changefiremode_from_";
	auto firemode = GetQueueSize();
	auto new_mode = m_iOldFireMode;
	if (new_mode < 0)
		anm_name += "a";
	else
		anm_name += std::to_string(new_mode);

	anm_name += "_to_";

	if (firemode < 0)
		anm_name += "a";
	else
		anm_name += std::to_string(firemode);

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && iAmmoElapsed == 0)
		{
			anm_name += "_detector";
			bIsNeedCallDet = true;
		}
		else
		{
			anm_name += "";
			bIsNeedCallDet = false;
		}
	}
	else
		bIsNeedCallDet = false;

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

void CWeaponMagazined::PlayAnimShowDet()
{
	VERIFY(GetState() ==  eShowingDet);

	std::string anm_name = "anm_prepare_detector";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

void CWeaponMagazined::PlayAnimShowEndDet()
{
    VERIFY(GetState() == eShowingEndDet);

	std::string anm_name = "anm_draw_detector";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), FALSE, this, GetState());
}

void CWeaponMagazined::PlayAnimHide()
{
	VERIFY(GetState() == eHiding);

	std::string anm_name = "anm_hide";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), FALSE, this, GetState());
}

void CWeaponMagazined::PlayAnimHideDet()
{
    VERIFY(GetState() == eHideDet);

	std::string anm_name = "anm_finish_detector";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

void CWeaponMagazined::PlayAnimReload()
{
	VERIFY(GetState() == eReload);

	std::string anm_name = "anm_reload";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else if (IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_jammed_last";
	else if (IsChangeAmmoType())
		anm_name += "_ammochange";
	else if(IsChangeAmmoType() && iAmmoElapsed == 0)
		anm_name += "_empty_ammochange";
	else
		anm_name += "";

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det)
		{
			anm_name += "_detector";
			bIsNeedCallDet = true;
		}
		else
		{
			anm_name += "";
			bIsNeedCallDet = false;
		}
	}
	else
		bIsNeedCallDet = false;

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

void CWeaponMagazined::PlayAnimAimStart()
{
    VERIFY(GetState() == eZoomStart);

	auto bm = dynamic_cast<CWeaponBM16*>(this);

	if(!bm)
	{
		std::string anm_name = "anm_idle_aim_start";
		auto firemode = GetQueueSize();

		if (firemode == -1 && m_sFireModeMask_a != nullptr)
			anm_name += m_sFireModeMask_a.c_str();
		else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
			anm_name += m_sFireModeMask_1.c_str();
		else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
			anm_name += m_sFireModeMask_3.c_str();
		else
			anm_name += "";

		if (!IsMisfire() && iAmmoElapsed == 0)
			anm_name += "_empty";
		else if (IsMisfire())
			anm_name += "_jammed";
		else
			anm_name += "";

		attachable_hud_item* i1 = g_player_hud->attached_item(1);
		if (i1 && HudItemData())
		{
			auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
			if (det)
				det->SwitchState(CCustomDetector::eZoomStartDet);
		}

		PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
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
	std::string anm_name = "anm_idle_moving";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleSprintStart()
{

	std::string anm_name = "anm_idle_sprint_start";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleSprint()
{
	std::string anm_name = "anm_idle_sprint";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleSprintEnd()
{
	std::string anm_name = "anm_idle_sprint_end";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleMovingCrouch()
{
	std::string anm_name = "anm_idle_moving_crouch";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleMovingCrouchSlow()
{
	std::string anm_name = "anm_idle_moving_crouch_slow";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimIdleMovingSlow()
{
	std::string anm_name = "anm_idle_moving_slow";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
}

void CWeaponMagazined::PlayAnimAimEnd()
{
    VERIFY(GetState() == eZoomEnd);

	auto bm = dynamic_cast<CWeaponBM16*>(this);

	if(!bm)
	{
		std::string anm_name = "anm_idle_aim_end";
		auto firemode = GetQueueSize();

		if (firemode == -1 && m_sFireModeMask_a != nullptr)
			anm_name += m_sFireModeMask_a.c_str();
		else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
			anm_name += m_sFireModeMask_1.c_str();
		else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
			anm_name += m_sFireModeMask_3.c_str();
		else
			anm_name += "";

		if (!IsMisfire() && iAmmoElapsed == 0)
			anm_name += "_empty";
		else if (IsMisfire())
			anm_name += "_jammed";
		else
			anm_name += "";

		attachable_hud_item* i1 = g_player_hud->attached_item(1);
		if (i1 && HudItemData())
		{
			auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
			if (det)
				det->SwitchState(CCustomDetector::eZoomEndDet);
		}

		PlayHUDMotion(anm_name.c_str(), TRUE, nullptr, GetState());
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

	std::string anm_name = "anm_fakeshoot";
	auto firemode = GetQueueSize();

	if(IsZoomed())
		anm_name += "_aim";
	else
		anm_name += "";

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	anm_name += "_empty";

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && (det->GetState() == CCustomDetector::eIdle || det->GetState() == CCustomDetector::eEmptyDet))
			det->SwitchState(CCustomDetector::eEmptyDet);
	}

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

const char* CWeaponMagazined::GetAnimAimName()
{
	auto pActor = dynamic_cast<const CActor*>(H_Parent());
	if (pActor)
	{
        u32 state = pActor->get_state();
        if (state & mcAnyMove)
		{
			std::string anm_name = "";
			auto firemode = GetQueueSize();

			if (firemode == -1 && m_sFireModeMask_a != nullptr)
				anm_name += m_sFireModeMask_a.c_str();
			else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
				anm_name += m_sFireModeMask_1.c_str();
			else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
				anm_name += m_sFireModeMask_3.c_str();
			else
				anm_name += "";

			if (IsScopeAttached())
			{
				strcpy_s(guns_aim_anm, "anm_idle_aim_scope_moving");
				return guns_aim_anm;
			}
			else
				return xr_strconcat(guns_aim_anm, "anm_idle_aim_moving", (state & mcFwd) ? "_forward" : ((state & mcBack) ? "_back" : ""), (state & mcLStrafe) ? "_left" : ((state & mcRStrafe) ? "_right" : ""), anm_name.c_str(), IsMisfire() ? "_jammed" : (!IsMisfire() && iAmmoElapsed == 0 ? "_empty" : ""));
		}
	}
	return nullptr;
}

void CWeaponMagazined::PlayAnimAim()
{
	if (const char* guns_aim_anm = GetAnimAimName())
		PlayHUDMotion(guns_aim_anm, true, this, GetState());
	else
	{
		std::string anm_name = "anm_idle_aim";
		auto firemode = GetQueueSize();

		if (firemode == -1 && m_sFireModeMask_a != nullptr)
			anm_name += m_sFireModeMask_a.c_str();
		else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
			anm_name += m_sFireModeMask_1.c_str();
		else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
			anm_name += m_sFireModeMask_3.c_str();
		else
			anm_name += "";

		if (!IsMisfire() && iAmmoElapsed == 0)
			anm_name += "_empty";
		else if (IsMisfire())
			anm_name += "_jammed";
		else
			anm_name += "";

		PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
	}
}

void CWeaponMagazined::PlayAnimIdle()
{
    VERIFY(GetState() == eIdle);

    if (TryPlayAnimIdle())
        return;

	if (!IsZoomed())
	{
		std::string anm_name = "anm_idle";
		auto firemode = GetQueueSize();

		if (firemode == -1 && m_sFireModeMask_a != nullptr)
			anm_name += m_sFireModeMask_a.c_str();
		else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
			anm_name += m_sFireModeMask_1.c_str();
		else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
			anm_name += m_sFireModeMask_3.c_str();
		else
			anm_name += "";

		if (!IsMisfire() && iAmmoElapsed == 0)
			anm_name += "_empty";
		else if (IsMisfire())
			anm_name += "_jammed";
		else
			anm_name += "";

		PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
	}
	else
		PlayAnimAim();
}

void CWeaponMagazined::PlayAnimBore()
{
	std::string anm_name = "anm_bore";
	auto firemode = GetQueueSize();

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (!IsMisfire() && iAmmoElapsed == 0)
		anm_name += "_empty";
	else if (IsMisfire())
		anm_name += "_jammed";
	else
		anm_name += "";

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

void CWeaponMagazined::PlayAnimShoot()
{
	VERIFY(GetState() == eFire);

	std::string anm_name = "anm_shoot";
	auto firemode = GetQueueSize();

	if (IsZoomed())
	{
		if(!IsScopeAttached())
			anm_name += "_aim";
		else
			anm_name += "_aim_scope";
	}
	else
		anm_name += "";

	if (firemode == -1 && m_sFireModeMask_a != nullptr)
		anm_name += m_sFireModeMask_a.c_str();
	else if (firemode == 1 && m_sFireModeMask_1 != nullptr)
		anm_name += m_sFireModeMask_1.c_str();
	else if (firemode == 3 && m_sFireModeMask_3 != nullptr)
		anm_name += m_sFireModeMask_3.c_str();
	else
		anm_name += "";

	if (IsMisfire())
		anm_name += "_jammed";
	else if (iAmmoElapsed == 1)
		anm_name += "_last";
	else
		anm_name += "";

	if (IsSilencerAttached())
		anm_name += "_sil";
	else
		anm_name += "";

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && (det->GetState() == CCustomDetector::eIdle || det->GetState() == CCustomDetector::eFireDet))
			det->SwitchState(CCustomDetector::eFireDet);
	}

	PlayHUDMotion(anm_name.c_str(), TRUE, this, GetState());
}

void CWeaponMagazined::OnZoomIn()
{
	inherited::OnZoomIn();

	if(GetState() == eIdle)
		PlayAnimIdle();


	CActor* pActor = dynamic_cast<CActor*>(H_Parent());
	if(pActor)
	{
		CEffectorZoomInertion* S = dynamic_cast<CEffectorZoomInertion*>	(pActor->Cameras().GetCamEffector(eCEZoom));
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

	CActor* pActor = dynamic_cast<CActor*>(H_Parent());

	if(pActor)
		pActor->Cameras().RemoveCamEffector	(eCEZoom);

}

float CWeaponMagazined::GetWeaponDeterioration()
{
	return (m_iShotNum==1) ? conditionDecreasePerShot : conditionDecreasePerQueueShot;
};

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
	if (!m_bHasDifferentFireModes)
		return;

	if (GetState() != eIdle)
		return;

	if (IsZoomed())
		return;

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && det->GetState() != CCustomDetector::eIdle)
			return;
	}

	m_iOldFireMode = m_iQueueSize;
	m_iCurFireMode = (m_iCurFireMode+1+m_aFireModes.size()) % m_aFireModes.size();
	SetQueueSize(GetCurrentFireMode());

	SwitchState(eSwitchMode);
};

void CWeaponMagazined::OnPrevFireMode()
{
	if (!m_bHasDifferentFireModes)
		return;

	if (GetState() != eIdle)
		return;

	if (IsZoomed())
		return;

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && det->GetState() != CCustomDetector::eIdle)
			return;
	}

	m_iOldFireMode = m_iQueueSize;
	m_iCurFireMode = (m_iCurFireMode-1+m_aFireModes.size()) % m_aFireModes.size();
	SetQueueSize(GetCurrentFireMode());

	SwitchState(eSwitchMode);
};

void CWeaponMagazined::OnH_A_Chield()
{
	if (m_bHasDifferentFireModes)
	{
		CActor	*actor = dynamic_cast<CActor*>(H_Parent());
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
	save_data(m_iOldFireMode, output_packet);
}

void CWeaponMagazined::load(IReader &input_packet)
{
	inherited::load	(input_packet);
	load_data(m_iQueueSize, input_packet);SetQueueSize(m_iQueueSize);
	load_data(m_iShotNum, input_packet);
	load_data(m_iCurFireMode, input_packet);
	load_data(m_iOldFireMode, input_packet);
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

void CWeaponMagazined::UpdateAddonsVisibility()
{
	inherited::UpdateAddonsVisibility();

	IKinematics* pWeaponVisual = dynamic_cast<IKinematics*>(Visual()); R_ASSERT(pWeaponVisual);

	u16 bone_id;

	UpdateHUDAddonsVisibility();
	pWeaponVisual->CalculateBones_Invalidate();

	auto firemode = GetQueueSize();

	for (const auto& boneName : m_sFireModeBonesTotal)
	{
		bone_id = pWeaponVisual->LL_BoneID(boneName);
		if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
			pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
	}

	if (firemode == 1)
	{
		for (const auto& boneName : m_sFireModeBone_1)
		{
			bone_id = pWeaponVisual->LL_BoneID(boneName);
			if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
		}
	}
	if (firemode == 3)
	{
		for (const auto& boneName : m_sFireModeBone_3)
		{
			bone_id = pWeaponVisual->LL_BoneID(boneName);
			if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
		}
	}
	if (firemode == -1)
	{
		for (const auto& boneName : m_sFireModeBone_a)
		{
			bone_id = pWeaponVisual->LL_BoneID(boneName);
			if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
		}
	}

	pWeaponVisual->CalculateBones_Invalidate();
	pWeaponVisual->CalculateBones(TRUE);
}

void CWeaponMagazined::UpdateHUDAddonsVisibility()
{
	if(!GetHUDmode())
		return;

	inherited::UpdateHUDAddonsVisibility();

	auto firemode = GetQueueSize();

	auto SetBoneVisible = [&](const shared_str& boneName, BOOL visibility)
	{
		HudItemData()->set_bone_visible(boneName, visibility, TRUE);
	};

	for (const shared_str& bone : m_sFireModeBonesTotal)
	{
		SetBoneVisible(bone, FALSE);
	}

	if (firemode == 1)
	{
		for (const shared_str& bone : m_sFireModeBone_1)
		{
			SetBoneVisible(bone, TRUE);
		}
	}

	if (firemode == 3)
	{
		for (const shared_str& bone : m_sFireModeBone_3)
		{
			SetBoneVisible(bone, TRUE);
		}
	}

	if (firemode == -1)
	{
		for (const shared_str& bone : m_sFireModeBone_a)
		{
			SetBoneVisible(bone, TRUE);
		}
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