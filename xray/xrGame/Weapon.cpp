#include "stdafx.h"
#include "Weapon.h"
#include "ParticlesObject.h"
#include "HUDManager.h"
#include "entity_alive.h"
#include "inventory_item_impl.h"
#include "inventory.h"
#include "xrserver_objects_alife_items.h"
#include "actor.h"
#include "actoreffector.h"
#include "level.h"
#include "xr_level_controller.h"
#include "game_cl_base.h"
#include "../Include/xrRender/Kinematics.h"
#include "ai_object_location.h"
#include "mathutils.h"
#include "object_broker.h"
#include "player_hud.h"
#include "gamepersistent.h"
#include "effectorFall.h"
#include "debug_renderer.h"
#include "static_cast_checked.hpp"
#include "clsid_game.h"
#include "ui/UIWindow.h"
#include "../xrEngine/LightAnimLibrary.h"
#include "WeaponBinoculars.h"
#include "WeaponMagazinedWGrenade.h"
#include "CustomDetector.h"

#define WEAPON_REMOVE_TIME		60000
#define ROTATION_TIME			0.25f

BOOL	b_toggle_weapon_aim		= FALSE;

CWeapon::CWeapon(): m_fLR_MovingFactor(0.f), m_strafe_offset{}
{
	SetState				(eHidden);
	SetNextState			(eHidden);
	m_sub_state				= eSubstateReloadBegin;
	m_bTriStateReload		= false;
	SetDefaults				();

	m_Offset.identity		();
	m_StrapOffset.identity	();

	iAmmoCurrent			= -1;
	m_dwAmmoCurrentCalcFrame= 0;

	iAmmoElapsed			= -1;
	iMagazineSize			= -1;
	m_ammoType				= 0;
	m_ammoName				= NULL;

	bAltOffset				= false;

	eHandDependence			= hdNone;

	m_zoom_params.m_fCurrentZoomFactor			= g_fov;
	m_zoom_params.m_fZoomRotationFactor			= 0.f;
	m_zoom_params.m_fSecondVPFovFactor = 0.0f;

	m_pAmmo					= NULL;


	m_pFlameParticles2		= NULL;
	m_sFlameParticles2		= NULL;


	m_fCurrentCartirdgeDisp = 1.f;

	m_strap_bone0			= 0;
	m_strap_bone1			= 0;
	m_StrapOffset.identity	();
	m_strapped_mode			= false;
	m_can_be_strapped		= false;
	m_ef_main_weapon_type	= u32(-1);
	m_ef_weapon_type		= u32(-1);
	m_UIScope				= NULL;
	m_set_next_ammoType_on_reload = u32(-1);
	m_crosshair_inertion	= 0.f;

	bIsNeedCallDet = false;

	m_fSafemodeRotationFactor  = 0.f;
}

CWeapon::~CWeapon()
{
	xr_delete(m_UIScope);
	
	flashlight_render.destroy();
	flashlight_omni.destroy();
	flashlight_glow.destroy();
}

void CWeapon::Hit(SHit* pHDS)
{
	inherited::Hit(pHDS);
}

void CWeapon::UpdateXForm()
{
	//if (Device.dwFrame == dwXF_Frame)
	//	return;
	//
	//dwXF_Frame				= Device.dwFrame;

	if (!H_Parent())
		return;

	// Get access to entity and its visual
	CEntityAlive*			E = dynamic_cast<CEntityAlive*>(H_Parent());
	
	if (!E) {
		if (!IsGameTypeSingle())
			UpdatePosition	(H_Parent()->XFORM());

		return;
	}

	const CInventoryOwner	*parent = dynamic_cast<const CInventoryOwner*>(E);
	if (parent && parent->use_simplified_visual())
		return;

	if (parent->attached(this))
		return;

	IKinematics*			V = dynamic_cast<IKinematics*>	(E->Visual());
	VERIFY					(V);

	// Get matrices
	int						boneL = -1, boneR = -1, boneR2 = -1;

	// this ugly case is possible in case of a CustomMonster, not a Stalker, nor an Actor
	E->g_WeaponBones		(boneL,boneR,boneR2);

	if (boneR == -1)		return;

	if ((HandDependence() == hd1Hand) || (GetState() == eReload) || (!E->g_Alive()))
		boneL				= boneR2;

	V->CalculateBones		();
	Fmatrix& mL				= V->LL_GetTransform(u16(boneL));
	Fmatrix& mR				= V->LL_GetTransform(u16(boneR));
	// Calculate
	Fmatrix					mRes;
	Fvector					R,D,N;
	D.sub					(mL.c,mR.c);	

	if(fis_zero(D.magnitude())) {
		mRes.set			(E->XFORM());
		mRes.c.set			(mR.c);
	}
	else {		
		D.normalize			();
		R.crossproduct		(mR.j,D);

		N.crossproduct		(D,R);			
		N.normalize			();

		mRes.set			(R,N,D,mR.c);
		mRes.mulA_43		(E->XFORM());
	}

	UpdatePosition			(mRes);
}

void CWeapon::UpdateFireDependencies_internal()
{
	if (Device.dwFrame!=dwFP_Frame) 
	{
		dwFP_Frame			= Device.dwFrame;

		UpdateXForm			();

		if ( GetHUDmode() )
		{
			HudItemData()->setup_firedeps		(m_current_firedeps);
			VERIFY(_valid(m_current_firedeps.m_FireParticlesXForm));
		} else 
		{
			// 3rd person or no parent
			Fmatrix& parent			= XFORM();
			Fvector& fp				= vLoadedFirePoint;
			Fvector& fp2			= vLoadedFirePoint2;
			Fvector& sp				= vLoadedShellPoint;

			parent.transform_tiny	(m_current_firedeps.vLastFP,fp);
			parent.transform_tiny	(m_current_firedeps.vLastFP2,fp2);
			parent.transform_tiny	(m_current_firedeps.vLastSP,sp);
			
			m_current_firedeps.vLastFD.set	(0.f,0.f,1.f);
			parent.transform_dir	(m_current_firedeps.vLastFD);

			m_current_firedeps.m_FireParticlesXForm.set(parent);
			VERIFY(_valid(m_current_firedeps.m_FireParticlesXForm));
		}
	}
}

void CWeapon::ForceUpdateFireParticles()
{
	if ( !GetHUDmode() )
	{//update particlesXFORM real bullet direction

		if (!H_Parent())		return;

		Fvector					p, d; 
		dynamic_cast<CEntity*>(H_Parent())->g_fireParams	(this, p,d);

		Fmatrix						_pxf;
		_pxf.k						= d;
		_pxf.i.crossproduct			(Fvector().set(0.0f,1.0f,0.0f),	_pxf.k);
		_pxf.j.crossproduct			(_pxf.k,		_pxf.i);
		_pxf.c						= XFORM().c;
		
		m_current_firedeps.m_FireParticlesXForm.set	(_pxf);
	}
}

shared_str wpn_handler = "wpn_handler";

void CWeapon::Load		(LPCSTR section)
{
	inherited::Load					(section);
	CShootingObject::Load			(section);

	
	if(pSettings->line_exist(section, "flame_particles_2"))
		m_sFlameParticles2 = pSettings->r_string(section, "flame_particles_2");

	// load ammo classes
	m_ammoTypes.clear	(); 
	LPCSTR				S = pSettings->r_string(section,"ammo_class");
	if (S && S[0]) 
	{
		string128		_ammoItem;
		int				count		= _GetItemCount	(S);
		for (int it=0; it<count; ++it)	
		{
			_GetItem				(S,it,_ammoItem);
			m_ammoTypes.push_back	(_ammoItem);
		}
		m_ammoName = pSettings->r_string(*m_ammoTypes[0],"inv_name_short");
	}
	else
		m_ammoName = 0;

	iAmmoElapsed		= pSettings->r_s32		(section,"ammo_elapsed"		);
	iMagazineSize		= pSettings->r_s32		(section,"ammo_mag_size"	);
	
	////////////////////////////////////////////////////
	// дисперсия стрельбы

	//подбрасывание камеры во время отдачи
	u8 rm = READ_IF_EXISTS( pSettings, r_u8, section, "cam_return", 1 );
	cam_recoil.ReturnMode = (rm == 1);
	
	rm = READ_IF_EXISTS( pSettings, r_u8, section, "cam_return_stop", 0 );
	cam_recoil.StopReturn = (rm == 1);

	float temp_f = 0.0f;
	temp_f					= pSettings->r_float( section,"cam_relax_speed" );
	cam_recoil.RelaxSpeed	= _abs( deg2rad( temp_f ) );
	VERIFY( !fis_zero(cam_recoil.RelaxSpeed) );
	if ( fis_zero(cam_recoil.RelaxSpeed) )
	{
		cam_recoil.RelaxSpeed = EPS_L;
	}

	cam_recoil.RelaxSpeed_AI = cam_recoil.RelaxSpeed;
	if ( pSettings->line_exist( section, "cam_relax_speed_ai" ) )
	{
		temp_f						= pSettings->r_float( section, "cam_relax_speed_ai" );
		cam_recoil.RelaxSpeed_AI	= _abs( deg2rad( temp_f ) );
		VERIFY( !fis_zero(cam_recoil.RelaxSpeed_AI) );
		if ( fis_zero(cam_recoil.RelaxSpeed_AI) )
		{
			cam_recoil.RelaxSpeed_AI = EPS_L;
		}
	}
	temp_f						= pSettings->r_float( section, "cam_max_angle" );
	cam_recoil.MaxAngleVert		= _abs( deg2rad( temp_f ) );
	VERIFY( !fis_zero(cam_recoil.MaxAngleVert) );
	if ( fis_zero(cam_recoil.MaxAngleVert) )
	{
		cam_recoil.MaxAngleVert = EPS;
	}
	
	temp_f						= pSettings->r_float( section, "cam_max_angle_horz" );
	cam_recoil.MaxAngleHorz		= _abs( deg2rad( temp_f ) );
	VERIFY( !fis_zero(cam_recoil.MaxAngleHorz) );
	if ( fis_zero(cam_recoil.MaxAngleHorz) )
	{
		cam_recoil.MaxAngleHorz = EPS;
	}
	
	temp_f						= pSettings->r_float( section, "cam_step_angle_horz" );
	cam_recoil.StepAngleHorz	= deg2rad( temp_f );
	
	cam_recoil.DispersionFrac	= _abs( READ_IF_EXISTS( pSettings, r_float, section, "cam_dispersion_frac", 0.7f ) );

	//подбрасывание камеры во время отдачи в режиме zoom ==> ironsight or scope
	//zoom_cam_recoil.Clone( cam_recoil ); ==== нельзя !!!!!!!!!!
	zoom_cam_recoil.RelaxSpeed		= cam_recoil.RelaxSpeed;
	zoom_cam_recoil.RelaxSpeed_AI	= cam_recoil.RelaxSpeed_AI;
	zoom_cam_recoil.DispersionFrac	= cam_recoil.DispersionFrac;
	zoom_cam_recoil.MaxAngleVert	= cam_recoil.MaxAngleVert;
	zoom_cam_recoil.MaxAngleHorz	= cam_recoil.MaxAngleHorz;
	zoom_cam_recoil.StepAngleHorz	= cam_recoil.StepAngleHorz;

	zoom_cam_recoil.ReturnMode		= cam_recoil.ReturnMode;
	zoom_cam_recoil.StopReturn		= cam_recoil.StopReturn;

	
	if ( pSettings->line_exist( section, "zoom_cam_relax_speed" ) )
	{
		zoom_cam_recoil.RelaxSpeed		= _abs( deg2rad( pSettings->r_float( section, "zoom_cam_relax_speed" ) ) );
		VERIFY( !fis_zero(zoom_cam_recoil.RelaxSpeed) );
		if ( fis_zero(zoom_cam_recoil.RelaxSpeed) )
		{
			zoom_cam_recoil.RelaxSpeed = EPS_L;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_relax_speed_ai" ) )
	{
		zoom_cam_recoil.RelaxSpeed_AI	= _abs( deg2rad( pSettings->r_float( section,"zoom_cam_relax_speed_ai" ) ) ); 
		VERIFY( !fis_zero(zoom_cam_recoil.RelaxSpeed_AI) );
		if ( fis_zero(zoom_cam_recoil.RelaxSpeed_AI) )
		{
			zoom_cam_recoil.RelaxSpeed_AI = EPS_L;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_max_angle" ) )
	{
		zoom_cam_recoil.MaxAngleVert	= _abs( deg2rad( pSettings->r_float( section, "zoom_cam_max_angle" ) ) );
		VERIFY( !fis_zero(zoom_cam_recoil.MaxAngleVert) );
		if ( fis_zero(zoom_cam_recoil.MaxAngleVert) )
		{
			zoom_cam_recoil.MaxAngleVert = EPS;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_max_angle_horz" ) )
	{
		zoom_cam_recoil.MaxAngleHorz	= _abs( deg2rad( pSettings->r_float( section, "zoom_cam_max_angle_horz" ) ) ); 
		VERIFY( !fis_zero(zoom_cam_recoil.MaxAngleHorz) );
		if ( fis_zero(zoom_cam_recoil.MaxAngleHorz) )
		{
			zoom_cam_recoil.MaxAngleHorz = EPS;
		}
	}
	if ( pSettings->line_exist( section, "zoom_cam_step_angle_horz" ) )	{
		zoom_cam_recoil.StepAngleHorz	= deg2rad( pSettings->r_float( section, "zoom_cam_step_angle_horz" ) ); 
	}
	if ( pSettings->line_exist( section, "zoom_cam_dispersion_frac" ) )	{
		zoom_cam_recoil.DispersionFrac	= _abs( pSettings->r_float( section, "zoom_cam_dispersion_frac" ) );
	}

	m_pdm.m_fPDM_disp_base			= pSettings->r_float( section, "PDM_disp_base"			);
	m_pdm.m_fPDM_disp_vel_factor	= pSettings->r_float( section, "PDM_disp_vel_factor"	);
	m_pdm.m_fPDM_disp_accel_factor	= pSettings->r_float( section, "PDM_disp_accel_factor"	);
	m_pdm.m_fPDM_disp_crouch		= pSettings->r_float( section, "PDM_disp_crouch"		);
	m_pdm.m_fPDM_disp_crouch_no_acc	= pSettings->r_float( section, "PDM_disp_crouch_no_acc" );
	m_crosshair_inertion			= READ_IF_EXISTS(pSettings, r_float, section, "crosshair_inertion",	5.91f);
	m_first_bullet_controller.load	(section);

	//Параметры для обычного клина, когда застревает гильза
	fireDispersionConditionFactor	= READ_IF_EXISTS(pSettings, r_float, section, "fire_dispersion_condition_factor", 1.0f);
	misfireStartCondition			= READ_IF_EXISTS(pSettings, r_float, section, "misfire_start_condition", 0.3f);
	misfireEndCondition				= READ_IF_EXISTS(pSettings, r_float, section, "misfire_end_condition", 0.f);
	misfireStartProbability			= READ_IF_EXISTS(pSettings, r_float, section, "misfire_start_prob", 0.f);
	misfireEndProbability			= READ_IF_EXISTS(pSettings, r_float, section, "misfire_end_prob", 0.f);//pSettings->r_float(section, "misfire_end_prob");
	conditionDecreasePerShot		= READ_IF_EXISTS(pSettings, r_float, section, "condition_shot_dec", 0.f);//pSettings->r_float(section,"condition_shot_dec"); 
	conditionDecreasePerQueueShot	= READ_IF_EXISTS(pSettings, r_float, section, "condition_queue_shot_dec", conditionDecreasePerShot); 

	//Параметры для осечки, когда затвор выебывается
	m_bUseLightMisfire = READ_IF_EXISTS(pSettings, r_bool, section, "use_light_misfire", false);
	if(m_bUseLightMisfire)
	{
		l_misfireStartCondition =   pSettings->r_float(section, "light_misfire_start_condition");
		l_misfireEndCondition =     pSettings->r_float(section, "light_misfire_end_condition");
		l_misfireStartProbability = pSettings->r_float(section, "light_misfire_start_probability");
		l_misfireEndProbability =   pSettings->r_float(section, "light_misfire_end_probability");
	}
		
	vLoadedFirePoint	= pSettings->r_fvector3		(section,"fire_point"		);
	
	if(pSettings->line_exist(section,"fire_point2")) 
		vLoadedFirePoint2= pSettings->r_fvector3	(section,"fire_point2");
	else 
		vLoadedFirePoint2= vLoadedFirePoint;

	// hands
	eHandDependence		= EHandDependence(pSettings->r_s32(section,"hand_dependence"));
	m_bIsSingleHanded	= true;
	if (pSettings->line_exist(section, "single_handed"))
		m_bIsSingleHanded	= !!pSettings->r_bool(section, "single_handed");
	// 
	m_fMinRadius		= pSettings->r_float		(section,"min_radius");
	m_fMaxRadius		= pSettings->r_float		(section,"max_radius");

	m_sWpn_flashlight_bone = READ_IF_EXISTS(pSettings, r_string, section, "torch_cone_bones", "");

	m_sHud_wpn_flashlight_bone = READ_IF_EXISTS(pSettings, r_string, hud_sect, "torch_cone_bones", m_sWpn_flashlight_bone);

	// информация о возможных апгрейдах и их визуализации в инвентаре
	m_eScopeStatus			 = (ALife::EWeaponAddonStatus)pSettings->r_s32(section,"scope_status");
	m_eSilencerStatus		 = (ALife::EWeaponAddonStatus)pSettings->r_s32(section,"silencer_status");
	m_eGrenadeLauncherStatus = (ALife::EWeaponAddonStatus)pSettings->r_s32(section,"grenade_launcher_status");
	m_eHandlerStatus		 = (ALife::EWeaponAddonStatus)pSettings->r_s32(section,"handler_status");

	m_zoom_params.m_bZoomEnabled		= !!pSettings->r_bool(section,"zoom_enabled");
	m_zoom_params.m_fZoomRotateTime		= pSettings->r_float(section,"zoom_rotate_time");

	if ( m_eScopeStatus == ALife::eAddonAttachable )
	{
		m_sScopeName = pSettings->r_string(section,"scope_name");
		m_iScopeX = pSettings->r_s32(section,"scope_x");
		m_iScopeY = pSettings->r_s32(section,"scope_y");
	}

    
	if ( m_eSilencerStatus == ALife::eAddonAttachable )
	{
		m_sSilencerName = pSettings->r_string(section,"silencer_name");
		m_iSilencerX = pSettings->r_s32(section,"silencer_x");
		m_iSilencerY = pSettings->r_s32(section,"silencer_y");
	}

    
	if ( m_eGrenadeLauncherStatus == ALife::eAddonAttachable )
	{
		m_sGrenadeLauncherName = pSettings->r_string(section,"grenade_launcher_name");
		m_iGrenadeLauncherX = pSettings->r_s32(section,"grenade_launcher_x");
		m_iGrenadeLauncherY = pSettings->r_s32(section,"grenade_launcher_y");
	}

	if ( m_eHandlerStatus == ALife::eAddonAttachable )
	{
		m_sHandlerName = pSettings->r_string(section,"handler_name");
		m_iHandlerX = pSettings->r_s32(section,"handler_x");
		m_iHandlerY = pSettings->r_s32(section,"handler_y");

		if (pSettings->line_exist(section, "handler_bone"))
            m_sHandler_bone = pSettings->r_string(section, "handler_bone");
        else
            m_sHandler_bone = wpn_handler;
	}

	InitAddons();
	if(pSettings->line_exist(section,"weapon_remove_time"))
		m_dwWeaponRemoveTime = pSettings->r_u32(section,"weapon_remove_time");
	else
		m_dwWeaponRemoveTime = WEAPON_REMOVE_TIME;

	if(pSettings->line_exist(section,"auto_spawn_ammo"))
		m_bAutoSpawnAmmo = pSettings->r_bool(section,"auto_spawn_ammo");
	else
		m_bAutoSpawnAmmo = TRUE;

	bool SWM_3D_SCOPES = READ_IF_EXISTS(pSettings, r_bool, "gameplay", "SWM_3D_scopes", false);

	if(SWM_3D_SCOPES)
		m_zoom_params.m_fSecondVPFovFactor = READ_IF_EXISTS(pSettings, r_float, section, "3d_fov", 0.0f);

	m_zoom_params.m_bHideCrosshairInZoom		= true;

	if(pSettings->line_exist(hud_sect, "zoom_hide_crosshair"))
		m_zoom_params.m_bHideCrosshairInZoom = !!pSettings->r_bool(hud_sect, "zoom_hide_crosshair");	

	Fvector			def_dof;
	def_dof.set		(-1,-1,-1);
	m_zoom_params.m_ZoomDof		= READ_IF_EXISTS(pSettings, r_fvector3, section, "zoom_dof", Fvector().set(-1,-1,-1));
	m_zoom_params.m_bZoomDofEnabled	= !def_dof.similar(m_zoom_params.m_ZoomDof);

	m_zoom_params.m_ReloadDof	= READ_IF_EXISTS(pSettings, r_fvector4, section, "reload_dof", Fvector4().set(-1,-1,-1,-1));


	m_bHasTracers			= READ_IF_EXISTS(pSettings, r_bool, section, "tracers", true);
	m_u8TracerColorID		= READ_IF_EXISTS(pSettings, r_u8, section, "tracers_color_ID", u8(-1));

	string256						temp;
	for (int i=egdNovice; i<egdCount; ++i) 
	{
		strconcat					(sizeof(temp),temp,"hit_probability_",get_token_name(difficulty_type_token,i));
		m_hit_probability[i]		= READ_IF_EXISTS(pSettings,r_float,section,temp,1.f);
	}
	// Added by Axel, to enable optional condition use on any item
	m_flags.set(FUsingCondition, READ_IF_EXISTS(pSettings, r_bool, section, "use_condition", true));
	
	if (!flashlight_render && pSettings->line_exist(section, "flashlight_section"))
	{

		flashlight_attach_bone = pSettings->r_string(section, "torch_light_bone");
		flashlight_attach_offset = Fvector{ pSettings->r_float(section, "torch_attach_offset_x"), pSettings->r_float(section, "torch_attach_offset_y"), pSettings->r_float(section, "torch_attach_offset_z") };
		flashlight_omni_attach_offset = Fvector{ pSettings->r_float(section, "torch_omni_attach_offset_x"), pSettings->r_float(section, "torch_omni_attach_offset_y"), pSettings->r_float(section, "torch_omni_attach_offset_z") };
		flashlight_world_attach_offset = Fvector{ pSettings->r_float(section, "torch_world_attach_offset_x"), pSettings->r_float(section, "torch_world_attach_offset_y"), pSettings->r_float(section, "torch_world_attach_offset_z") };
		flashlight_omni_world_attach_offset = Fvector{ pSettings->r_float(section, "torch_omni_world_attach_offset_x"), pSettings->r_float(section, "torch_omni_world_attach_offset_y"), pSettings->r_float(section, "torch_omni_world_attach_offset_z") };
		
		has_flashlight = true;

		const bool b_r2 = psDeviceFlags.test(rsR2) || psDeviceFlags.test(rsR3);

		const char* m_light_section = pSettings->r_string(section, "flashlight_section");

		flashlight_lanim = LALib.FindItem(READ_IF_EXISTS(pSettings, r_string, m_light_section, "color_animator", ""));

		flashlight_render = ::Render->light_create();
		flashlight_render->set_type(IRender_Light::SPOT);
		flashlight_render->set_shadow(true);

		const Fcolor clr = READ_IF_EXISTS(pSettings, r_fcolor, m_light_section, b_r2 ? "color_r2" : "color", (Fcolor{ 0.6f, 0.55f, 0.55f, 1.0f }));
		flashlight_fBrightness = clr.intensity();
		flashlight_render->set_color(clr);
		const float range = READ_IF_EXISTS(pSettings, r_float, m_light_section, b_r2 ? "range_r2" : "range", 50.f);
		flashlight_render->set_range(range);
		flashlight_render->set_cone(deg2rad(READ_IF_EXISTS(pSettings, r_float, m_light_section, "spot_angle", 60.f)));
		flashlight_render->set_texture(READ_IF_EXISTS(pSettings, r_string, m_light_section, "spot_texture", nullptr));

		flashlight_omni = ::Render->light_create();
		flashlight_omni->set_type((IRender_Light::LT)(READ_IF_EXISTS(pSettings, r_u8, m_light_section, "omni_type", 2))); //KRodin: вообще omni это обычно поинт, но поинт светит во все стороны от себя, поэтому тут спот используется по умолчанию.
		flashlight_omni->set_shadow(false);

		const Fcolor oclr = READ_IF_EXISTS(pSettings, r_fcolor, m_light_section, b_r2 ? "omni_color_r2" : "omni_color", (Fcolor{ 1.0f , 1.0f , 1.0f , 0.0f }));
		flashlight_omni->set_color(oclr);
		const float orange = READ_IF_EXISTS(pSettings, r_float, m_light_section, b_r2 ? "omni_range_r2" : "omni_range", 0.25f);
		flashlight_omni->set_range(orange);

		flashlight_glow = ::Render->glow_create();
		flashlight_glow->set_texture(READ_IF_EXISTS(pSettings, r_string, m_light_section, "glow_texture", "glow\\glow_torch_r2"));
		flashlight_glow->set_color(clr);
		flashlight_glow->set_radius(READ_IF_EXISTS(pSettings, r_float, m_light_section, "glow_radius", 0.3f));
	}
	hud_recalc_koef = READ_IF_EXISTS(pSettings, r_float, hud_sect, "hud_recalc_koef", 1.35f); //На калаше при 1.35 вроде норм смотрится, другим стволам возможно придется подбирать другие значения.

	
	auto LoadBoneNames = [](pcstr section, pcstr line, RStringVec& list)
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
	
	LoadBoneNames(section, "def_show_bones", m_defShownBones);//Показ костей по дефолту
	
	LoadBoneNames(section, "def_hide_bones", m_defHiddenBones);//Скрытие костей по дефолту
	
	LoadBoneNames(section, "def_hide_bones_override_when_gl_attached", m_defGLHiddenBones);//Скрытие костей по дефолту с надетым ПГ

	LoadBoneNames(section, "hide_bones_override_when_silencer_attached", m_defSilHiddenBones);//Скрытие костей по дефолту с надетым глушителем

	LoadBoneNames(section, "collimator_sights_bones", m_colimSightBones);

	m_bUseDynamicZoom = READ_IF_EXISTS(pSettings, r_bool, section, "scope_dynamic_zoom", false);

	m_bUseLowAmmoSnd = READ_IF_EXISTS(pSettings, r_bool, section, "use_lowammo_snd", false);

	m_iACPlaySnd = READ_IF_EXISTS(pSettings, r_s32, section, "lowammo_snd_ammo_count", 0);

	m_bUseAltScope = READ_IF_EXISTS(pSettings, r_bool, hud_sect, "alt_scope_enabled", false);

	m_bHideMarkInAlt = READ_IF_EXISTS(pSettings, r_bool, section, "hide_collimator_sights_in_alter_zoom", false);

	m_bJamNotShot = READ_IF_EXISTS(pSettings, r_bool, hud_sect, "no_jam_fire", false);

	m_bRestGL_and_Sil = READ_IF_EXISTS(pSettings, r_bool, section, "restricted_gl_and_sil", false);

	m_bUseSilHud = READ_IF_EXISTS(pSettings, r_bool, section, "use_sil_hud", false);

	m_bCanBeLowered = pSettings->r_bool(section, "can_be_lowered");

	if (m_bCanBeLowered)
		m_fSafemodeRotateTime = pSettings->r_float(section, "safemode_rotate_time");

	if (pSettings->line_exist(hud_sect, "disable_light_misfires_with_detector"))
		m_bDisableLMDet = pSettings->r_bool(hud_sect, "disable_light_misfires_with_detector");

	////////////////////////////////////////////
	//--#SM+# Begin--
	string16 _prefix = { "" };

	string128 val_name;

	// Смещение в стрейфе
	m_strafe_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, strconcat(sizeof(val_name), val_name, "strafe_hud_offset_pos", _prefix), Fvector().set(0.015f, 0.f, 0.f));
	m_strafe_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, strconcat(sizeof(val_name), val_name, "strafe_hud_offset_rot", _prefix), Fvector().set(0.f, 0.f, 4.5f));

	// Поворот в стрейфе
	m_strafe_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, strconcat(sizeof(val_name), val_name, "strafe_aim_hud_offset_pos", _prefix), Fvector().set(0.005f, 0.f, 0.f));
	m_strafe_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, strconcat(sizeof(val_name), val_name, "strafe_aim_hud_offset_rot", _prefix), Fvector().set(0.f, 0.f, 2.5f));

	// Параметры стрейфа
	float fFullStrafeTime     = READ_IF_EXISTS(pSettings, r_float, section, "strafe_transition_time", 0.25f);
	float fFullStrafeTime_aim = READ_IF_EXISTS(pSettings, r_float, section, "strafe_aim_transition_time", 0.15f);
	bool bStrafeEnabled       = READ_IF_EXISTS(pSettings, r_bool, section, "strafe_enabled", true);
	bool bStrafeEnabled_aim   = READ_IF_EXISTS(pSettings, r_bool, section, "strafe_aim_enabled", false);

	m_strafe_offset[2][0].set(bStrafeEnabled, fFullStrafeTime, 0.f); // normal
	m_strafe_offset[2][1].set(bStrafeEnabled_aim, fFullStrafeTime_aim, 0.f); // aim-GL
	//--#SM+# End--
	////////////////////////////////////////////
}

void CWeapon::LoadFireParams		(LPCSTR section)
{
	cam_recoil.Dispersion = deg2rad( pSettings->r_float( section,"cam_dispersion" ) ); 
	cam_recoil.DispersionInc = 0.0f;

	if ( pSettings->line_exist( section, "cam_dispersion_inc" ) )	{
		cam_recoil.DispersionInc = deg2rad( pSettings->r_float( section, "cam_dispersion_inc" ) ); 
	}
	
	zoom_cam_recoil.Dispersion		= cam_recoil.Dispersion;
	zoom_cam_recoil.DispersionInc	= cam_recoil.DispersionInc;

	if ( pSettings->line_exist( section, "zoom_cam_dispersion" ) )	{
		zoom_cam_recoil.Dispersion		= deg2rad( pSettings->r_float( section, "zoom_cam_dispersion" ) ); 
	}
	if ( pSettings->line_exist( section, "zoom_cam_dispersion_inc" ) )	{
		zoom_cam_recoil.DispersionInc	= deg2rad( pSettings->r_float( section, "zoom_cam_dispersion_inc" ) ); 
	}

	CShootingObject::LoadFireParams(section);
};



BOOL CWeapon::net_Spawn		(CSE_Abstract* DC)
{
	m_fRTZoomFactor					= m_zoom_params.m_fScopeZoomFactor;
	BOOL bResult					= inherited::net_Spawn(DC);
	CSE_Abstract					*e	= (CSE_Abstract*)(DC);
	CSE_ALifeItemWeapon			    *E	= dynamic_cast<CSE_ALifeItemWeapon*>(e);

	//iAmmoCurrent					= E->a_current;
	iAmmoElapsed					= E->a_elapsed;
	m_flagsAddOnState				= E->m_addon_flags.get();
	m_ammoType						= E->ammo_type;
	SetState						(E->wpn_state);
	SetNextState					(E->wpn_state);
	
	m_DefaultCartridge.Load(*m_ammoTypes[m_ammoType], u8(m_ammoType));	
	if(iAmmoElapsed) 
	{
		m_fCurrentCartirdgeDisp = m_DefaultCartridge.param_s.kDisp;
		for(int i = 0; i < iAmmoElapsed; ++i) 
			m_magazine.push_back(m_DefaultCartridge);
	}

	UpdateAddonsVisibility();
	InitAddons();

	m_dwWeaponIndependencyTime = 0;

	VERIFY((u32)iAmmoElapsed == m_magazine.size());
	m_bAmmoWasSpawned		= false;

	return bResult;
}

void CWeapon::net_Destroy	()
{
	inherited::net_Destroy	();

	//удалить объекты партиклов
	StopFlameParticles	();
	StopFlameParticles2	();
	StopLight			();
	Light_Destroy		();

	while (m_magazine.size()) m_magazine.pop_back();
}

BOOL CWeapon::IsUpdating()
{	
	bool bIsActiveItem = m_pInventory && m_pInventory->ActiveItem()==this;
	return bIsActiveItem || bWorking || IsPending() || getVisible();
}

void CWeapon::net_Export(NET_Packet& P)
{
	inherited::net_Export	(P);

	P.w_float_q8			(GetCondition(),0.0f,1.0f);


	u8 need_upd				= IsUpdating() ? 1 : 0;
	P.w_u8					(need_upd);
	P.w_u16					(u16(iAmmoElapsed));
	P.w_u8					(m_flagsAddOnState);
	P.w_u8					((u8)m_ammoType);
	P.w_u8					((u8)GetState());
	P.w_u8					((u8)IsZoomed());
}

void CWeapon::net_Import(NET_Packet& P)
{
	inherited::net_Import (P);
	
	float _cond;
	P.r_float_q8			(_cond,0.0f,1.0f);
	SetCondition			(_cond);

	u8 flags				= 0;
	P.r_u8					(flags);

	u16 ammo_elapsed = 0;
	P.r_u16					(ammo_elapsed);

	u8						NewAddonState;
	P.r_u8					(NewAddonState);

	m_flagsAddOnState		= NewAddonState;
	UpdateAddonsVisibility	();

	u8 ammoType, wstate;
	P.r_u8					(ammoType);
	P.r_u8					(wstate);

	u8 Zoom;
	P.r_u8					(Zoom);

	if (H_Parent() && H_Parent()->Remote())
	{
		if (Zoom) OnZoomIn();
		else OnZoomOut();
	};
	switch (wstate)
	{	
	case eFire:
	case eFire2:
	case eSwitch:
	case eReload:
		{
		}break;	
	default:
		{
			if (ammoType >= m_ammoTypes.size())
				Msg("!! Weapon [%d], State - [%d]", ID(), wstate);
			else
			{
				m_ammoType = ammoType;
				SetAmmoElapsed((ammo_elapsed));
			}
		}break;
	}
	
	VERIFY((u32)iAmmoElapsed == m_magazine.size());
}

void CWeapon::save(NET_Packet &output_packet)
{
	inherited::save	(output_packet);
	save_data		(iAmmoElapsed,					output_packet);
	save_data		(m_flagsAddOnState, 			output_packet);
	save_data		(m_ammoType,					output_packet);
	save_data		(bAltOffset,					output_packet);
	save_data		(m_zoom_params.m_bIsZoomModeNow,output_packet);
}

void CWeapon::load(IReader &input_packet)
{
	inherited::load	(input_packet);
	load_data		(iAmmoElapsed,					input_packet);
	load_data		(m_flagsAddOnState,				input_packet);
	UpdateAddonsVisibility			();
	load_data		(m_ammoType,					input_packet);
	load_data		(bAltOffset,					input_packet);
	load_data		(m_zoom_params.m_bIsZoomModeNow,input_packet);

	if (m_zoom_params.m_bIsZoomModeNow)	
			OnZoomIn();
		else			
			OnZoomOut();
}


void CWeapon::OnEvent(NET_Packet& P, u16 type) 
{
	switch (type)
	{
	case GE_ADDON_CHANGE:
		{
			P.r_u8					(m_flagsAddOnState);
			InitAddons();
			UpdateAddonsVisibility();
		}break;

	case GE_WPN_STATE_CHANGE:
		{
			u8				state;
			P.r_u8			(state);
			P.r_u8			(m_sub_state);		
//			u8 NewAmmoType = 
				P.r_u8();
			u8 AmmoElapsed = P.r_u8();
			u8 NextAmmo = P.r_u8();
			if (NextAmmo == u8(-1))
				m_set_next_ammoType_on_reload = u32(-1);
			else
				m_set_next_ammoType_on_reload = u8(NextAmmo);

			if (OnClient()) SetAmmoElapsed(int(AmmoElapsed));			
			OnStateSwitch	(u32(state));
		}
		break;
	default:
		{
			inherited::OnEvent(P,type);
		}break;
	}
};

void CWeapon::shedule_Update	(u32 dT)
{
	// Queue shrink
//	u32	dwTimeCL		= Level().timeServer()-NET_Latency;
//	while ((NET.size()>2) && (NET[1].dwTimeStamp<dwTimeCL)) NET.pop_front();	

	// Inherited
	inherited::shedule_Update	(dT);
}

void CWeapon::OnH_B_Independent	(bool just_before_destroy)
{
	RemoveShotEffector			();

	inherited::OnH_B_Independent(just_before_destroy);

	FireEnd						();
	SetPending					(FALSE);
	SwitchState					(eHidden);

	m_strapped_mode				= false;
	m_zoom_params.m_bIsZoomModeNow	= false;
	UpdateXForm					();
	m_fSafemodeRotationFactor	= 0.f;

	if (ParentIsActor())
	{
		auto active = Actor()->inventory().ActiveItem();
		if (this == active)
			Actor()->SetSafemodeStatus(false);
	}
}

void CWeapon::OnH_A_Independent	()
{
	m_dwWeaponIndependencyTime = Level().timeServer();
	inherited::OnH_A_Independent();
	Light_Destroy				();
	UpdateAddonsVisibility		();
};

void CWeapon::OnH_A_Chield		()
{
	inherited::OnH_A_Chield		();
	UpdateAddonsVisibility		();
};

void CWeapon::OnActiveItem()
{
	//. from Activate
	UpdateAddonsVisibility();
	m_dwAmmoCurrentCalcFrame = 0;

//. Show
	SwitchState					(eShowing);
//-

	if (ParentIsActor())
		Actor()->SetSafemodeStatus(false);

	inherited::OnActiveItem();
}

void CWeapon::OnHiddenItem ()
{
	m_dwAmmoCurrentCalcFrame = 0;
//. Hide
	if(IsGameTypeSingle())
		SwitchState(eHiding);
	else
		SwitchState(eHidden);
	OnZoomOut();
//-
	inherited::OnHiddenItem		();

	if (ParentIsActor())
		Actor()->SetSafemodeStatus(false);

	m_set_next_ammoType_on_reload = u32(-1);
}

void CWeapon::SendHiddenItem()
{
	if (!CHudItem::object().getDestroy() && m_pInventory)
	{
		// !!! Just single entry for given state !!!
		NET_Packet		P;
		CHudItem::object().u_EventGen		(P,GE_WPN_STATE_CHANGE,CHudItem::object().ID());
		P.w_u8			(u8(eHiding));
		P.w_u8			(u8(m_sub_state));
		P.w_u8			(u8(m_ammoType& 0xff));
		P.w_u8			(u8(iAmmoElapsed & 0xff));
		P.w_u8			(u8(m_set_next_ammoType_on_reload & 0xff));
		CHudItem::object().u_EventSend		(P, net_flags(TRUE, TRUE, FALSE, TRUE));
		SetPending		(TRUE);
	}
}


void CWeapon::OnH_B_Chield		()
{
	m_dwWeaponIndependencyTime = 0;
	inherited::OnH_B_Chield		();

	OnZoomOut					();
	m_set_next_ammoType_on_reload	= u32(-1);
}

extern u32 hud_adj_mode;

void CWeapon::UpdateCL		()
{
	inherited::UpdateCL		();
	UpdateHUDAddonsVisibility();
	//подсветка от выстрела
	UpdateLight				();

	//нарисовать партиклы
	UpdateFlameParticles	();
	UpdateFlameParticles2	();

	if(!IsGameTypeSingle())
		make_Interpolation		();

	if (!b_toggle_weapon_aim && IsZoomed() && GetState() == eIdle && !bZoomKeyPressed)
	{
		OnZoomOut();
		auto binoc = dynamic_cast<CWeaponBinoculars*>(Actor()->inventory().ActiveItem());
		if(!binoc)
			SwitchState(eZoomEnd);
	}

	if (!m_bDisableBore && !Actor()->IsSafemode() && (GetNextState() == GetState()) && IsGameTypeSingle() && H_Parent() == Level().CurrentEntity())
	{
		CActor* pActor	= dynamic_cast<CActor*>(H_Parent());
		if(pActor && !pActor->AnyMove() && this==pActor->inventory().ActiveItem())
		{
			if (hud_adj_mode==0 && GetState()==eIdle && (Device.dwTimeGlobal-m_dw_curr_substate_time>20000) && !IsZoomed() && g_player_hud->attached_item(1)==NULL)
			{
				SwitchState			(eBore);
				ResetSubStateTime	();
			}
		}
	}
	UpdateFlashlight();
}

void CWeapon::GetBoneOffsetPosDir(const shared_str& bone_name, Fvector& dest_pos, Fvector& dest_dir, const Fvector& offset) {
	const u16 bone_id = HudItemData()->m_model->LL_BoneID(bone_name);
	Fmatrix& fire_mat = HudItemData()->m_model->LL_GetTransform(bone_id);
	fire_mat.transform_tiny(dest_pos, offset);
	HudItemData()->m_item_transform.transform_tiny(dest_pos);
	dest_pos.add(Device.vCameraPosition);
	dest_dir.set(0.f, 0.f, 1.f);
	HudItemData()->m_item_transform.transform_dir(dest_dir);
}

void CWeapon::CorrectDirFromWorldToHud(Fvector& dir) {
	const auto& CamDir = Device.vCameraDirection;
	const float Fov = Device.fFOV;
	extern ENGINE_API float psHUD_FOV;
	const float HudFov = psHUD_FOV < 1.f ? psHUD_FOV * Device.fFOV : psHUD_FOV;
	const float diff = hud_recalc_koef * Fov / HudFov;
	dir.sub(CamDir);
	dir.mul(diff);
	dir.add(CamDir);
	dir.normalize();
}

void CWeapon::UpdateFlashlight()
{
	if (flashlight_render)
	{
		auto io = dynamic_cast<CInventoryOwner*>(H_Parent());
		if (!flashlight_render->get_active() && IsFlashlightOn() && (!H_Parent() || (io && this == io->inventory().ActiveItem()))) {
			flashlight_render->set_active(true);
			flashlight_omni->set_active(true);
			flashlight_glow->set_active(true);
			UpdateAddonsVisibility();
		}
		else if (flashlight_render->get_active() && (!IsFlashlightOn() || !(!H_Parent() || (io && this == io->inventory().ActiveItem())))) {
			flashlight_render->set_active(false);
			flashlight_omni->set_active(false);
			flashlight_glow->set_active(false);
			UpdateAddonsVisibility();
		}

		if (flashlight_render->get_active()) {
			Fvector flashlight_pos, flashlight_pos_omni, flashlight_dir, flashlight_dir_omni;

			if (GetHUDmode()) {
				GetBoneOffsetPosDir(flashlight_attach_bone, flashlight_pos, flashlight_dir, flashlight_attach_offset);
				CorrectDirFromWorldToHud(flashlight_dir);

				GetBoneOffsetPosDir(flashlight_attach_bone, flashlight_pos_omni, flashlight_dir_omni, flashlight_omni_attach_offset);
				CorrectDirFromWorldToHud(flashlight_dir_omni);
			}
			else {
				flashlight_dir = get_LastFD();
				XFORM().transform_tiny(flashlight_pos, flashlight_world_attach_offset);

				flashlight_dir_omni = get_LastFD();
				XFORM().transform_tiny(flashlight_pos_omni, flashlight_omni_world_attach_offset);
			}

			Fmatrix flashlightXForm;
			flashlightXForm.identity();
			flashlightXForm.k.set(flashlight_dir);
			Fvector::generate_orthonormal_basis_normalized(flashlightXForm.k, flashlightXForm.j, flashlightXForm.i);
			flashlight_render->set_position(flashlight_pos);
			flashlight_render->set_rotation(flashlightXForm.k, flashlightXForm.i);

			flashlight_glow->set_position(flashlight_pos);
			flashlight_glow->set_direction(flashlightXForm.k);

			Fmatrix flashlightomniXForm;
			flashlightomniXForm.identity();
			flashlightomniXForm.k.set(flashlight_dir_omni);
			Fvector::generate_orthonormal_basis_normalized(flashlightomniXForm.k, flashlightomniXForm.j, flashlightomniXForm.i);
			flashlight_omni->set_position(flashlight_pos_omni);
			flashlight_omni->set_rotation(flashlightomniXForm.k, flashlightomniXForm.i);

			// calc color animator
			if (flashlight_lanim)
			{
				int frame;
				const u32 clr = flashlight_lanim->CalculateBGR(Device.fTimeGlobal, frame);

				Fcolor fclr{ (float)color_get_B(clr), (float)color_get_G(clr), (float)color_get_R(clr), 1.f };
				fclr.mul_rgb(flashlight_fBrightness / 255.f);
				flashlight_render->set_color(fclr);
				flashlight_omni->set_color(fclr);
				flashlight_glow->set_color(fclr);
			}
		}
	}
}

bool CWeapon::need_renderable()
{
	return !Device.m_SecondViewport.IsSVPFrame() && !(IsZoomed() && ZoomTexture() && !IsRotatingToZoom());
}

void CWeapon::renderable_Render		()
{
	UpdateXForm				();

	//нарисовать подсветку

	RenderLight				();	

	//если мы в режиме снайперки, то сам HUD рисовать не надо
	if(IsZoomed() && !IsRotatingToZoom() && ZoomTexture() && !bAltOffset)
		RenderHud		(FALSE);
	else
		RenderHud		(TRUE);

	inherited::renderable_Render	();
}

void CWeapon::signal_HideComplete()
{
	if(H_Parent()) 
		setVisible			(FALSE);
	SetPending				(FALSE);
}

void CWeapon::SetDefaults()
{
	SetPending			(FALSE);

	m_flags.set			(FUsingCondition, TRUE);
	bMisfire			= false;
	m_flagsAddOnState	= 0;
	m_zoom_params.m_bIsZoomModeNow	= false;
}

void CWeapon::UpdatePosition(const Fmatrix& trans)
{
	Position().set		(trans.c);
	XFORM().mul			(trans,m_strapped_mode ? m_StrapOffset : m_Offset);
	VERIFY				(!fis_zero(DET(renderable.xform)));
}


bool CWeapon::Action(s32 cmd, u32 flags) 
{
	if(inherited::Action(cmd, flags)) return true;

	switch(cmd) 
	{
		case kWPN_FIRE: 
			{
				//если оружие чем-то занято, то ничего не делать
				{				
					if(IsPending())		
						return false;

					if (ParentIsActor() && Actor()->IsSafemode())
					{
						Actor()->SetSafemodeStatus(false);
						return false;
					}

					if(flags&CMD_START) 
						FireStart();
					else 
						FireEnd();
				};
			} 
			return true;
		case kWPN_NEXT: 
			return SwitchAmmoType(flags);
		case kWPN_ZOOM:
			return TryZoom(flags);
		case kWPN_ALT_ZOOM:
			if(m_bUseAltScope && !IsGrenadeLauncherMode())
			{
				if(flags&CMD_START)
				{
					bAltOffset = !bAltOffset;
					InitAddons();
					return true;
				}
				else
					return false;
			}
			else
				return false;
		case kSAFEMODE:
			{
				if (!m_bCanBeLowered)
					return false;

				if (IsPending())
					return false;

				if (GetState() != eIdle)
					return false;

				if (IsZoomed())
					return false;

				auto state = Actor()->get_state();
				if (state & mcSprint)
					return false;

				if (flags&CMD_START)
				{
					if(Actor() && Actor()->IsSafemode())
						Actor()->SetSafemodeStatus(false);
					else
						Actor()->SetSafemodeStatus(true);

					ResetSubStateTime();
				}
			}
			return true;
		case kWPN_ZOOM_INC:
		case kWPN_ZOOM_DEC:
			if(IsZoomEnabled() && IsZoomed() && (flags&CMD_START))
			{
				if(cmd==kWPN_ZOOM_INC)
					ZoomInc();
				else
					ZoomDec();
				return true;
			}
			else
				return false;
	}
	return false;
}

bool CWeapon::TryZoom(u32 flags)
{
	if (!IsZoomEnabled())
		return false;

	auto binoc = dynamic_cast<CWeaponBinoculars*>(Actor()->inventory().ActiveItem());

	if (!IsZoomed() && ParentIsActor() && Actor()->IsSafemode())
	{
		Actor()->SetSafemodeStatus(false);
		return false;
	}

	if (b_toggle_weapon_aim)
	{
		if(flags & CMD_START)
		{
			if(!IsPending())
			{
				if(!IsZoomed())
				{
					OnZoomIn();
					if(!binoc)
						SwitchState(eZoomStart);
				}
				else
				{
					OnZoomOut();
					if(!binoc)
						SwitchState(eZoomEnd);
				}
			}
		}
	}
	else
	{
		if (flags & CMD_START)
		{
			bZoomKeyPressed = true;
			if(!IsPending())
			{
				if(!IsZoomed())
				{
					OnZoomIn();
					if(GetState() == eFire)
						FireEnd();
					if(!binoc)
						SwitchState(eZoomStart);
				}
			}
		}
		else
		{
			bZoomKeyPressed = false;
			if(GetState() == eFire)
				return false;
			if(!IsPending())
			{
				if(IsZoomed())
				{
					OnZoomOut();
					if(!binoc)
						SwitchState(eZoomEnd);
				}
			}
		}
	}

	return true;
}

bool CWeapon::SwitchAmmoType(u32 flags) 
{
	if (IsPending() || OnClient())
		return false;

	if (!(flags & CMD_START))
		return false;

	if (ParentIsActor() && Actor()->IsSafemode())
	{
        Actor()->SetSafemodeStatus(false);
        return false;
    }

	if(IsZoomed())
		return false;

	attachable_hud_item* i1 = g_player_hud->attached_item(1);
	if (i1 && HudItemData())
	{
		auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
		if (det && (det->GetState() != CCustomDetector::eIdle || det->m_bNeedActivation))
			return false;
	}

	if (IsMisfire() && !IsGrenadeLauncherMode())
	{
		Reload();
		return false;
	}

	u32 l_newType = m_ammoType;
	bool b1, b2;
	do 
	{
		l_newType = (l_newType+1) % m_ammoTypes.size();
		b1 = l_newType != m_ammoType;
		b2 = unlimited_ammo() ? false : ( !m_pInventory->GetAny( *m_ammoTypes[l_newType] ) );						
	} while( b1 && b2 );

	if ( l_newType != m_ammoType )
	{
		m_set_next_ammoType_on_reload = l_newType;					
		if ( OnServer() )
		{
			Reload();
		}
	}

	return true;
}

void CWeapon::SpawnAmmo(u32 boxCurr, LPCSTR ammoSect, u32 ParentID) 
{
	if(!m_ammoTypes.size())			return;
	if (OnClient())					return;
	m_bAmmoWasSpawned				= true;
	
	int l_type						= 0;
	l_type							%= m_ammoTypes.size();

	if(!ammoSect) ammoSect			= *m_ammoTypes[l_type]; 
	
	++l_type; 
	l_type							%= m_ammoTypes.size();

	CSE_Abstract *D					= F_entity_Create(ammoSect);

	if (D->m_tClassID==CLSID_OBJECT_AMMO	||
		D->m_tClassID==CLSID_OBJECT_A_M209	||
		D->m_tClassID==CLSID_OBJECT_A_VOG25	||
		D->m_tClassID==CLSID_OBJECT_A_OG7B)
	{	
		CSE_ALifeItemAmmo *l_pA		= dynamic_cast<CSE_ALifeItemAmmo*>(D);
		R_ASSERT					(l_pA);
		l_pA->m_boxSize				= (u16)pSettings->r_s32(ammoSect, "box_size");
		D->s_name					= ammoSect;
		D->set_name_replace			("");
//.		D->s_gameid					= u8(GameID());
		D->s_RP						= 0xff;
		D->ID						= 0xffff;
		if (ParentID == 0xffffffff)	
			D->ID_Parent			= (u16)H_Parent()->ID();
		else
			D->ID_Parent			= (u16)ParentID;

		D->ID_Phantom				= 0xffff;
		D->s_flags.assign			(M_SPAWN_OBJECT_LOCAL);
		D->RespawnTime				= 0;
		l_pA->m_tNodeID				= g_dedicated_server ? u32(-1) : ai_location().level_vertex_id();

		if(boxCurr == 0xffffffff) 	
			boxCurr					= l_pA->m_boxSize;

		while(boxCurr) 
		{
			l_pA->a_elapsed			= (u16)(boxCurr > l_pA->m_boxSize ? l_pA->m_boxSize : boxCurr);
			NET_Packet				P;
			D->Spawn_Write			(P, TRUE);
			Level().Send			(P,net_flags(TRUE));

			if(boxCurr > l_pA->m_boxSize) 
				boxCurr				-= l_pA->m_boxSize;
			else 
				boxCurr				= 0;
		}
	};
	F_entity_Destroy				(D);
}

int CWeapon::GetSuitableAmmoTotal(bool use_item_to_spawn) const
{
	int l_count = iAmmoElapsed;
	if(!m_pInventory) return l_count;

	//чтоб не делать лишних пересчетов
	if(m_pInventory->ModifyFrame()<=m_dwAmmoCurrentCalcFrame)
		return l_count + iAmmoCurrent;

 	m_dwAmmoCurrentCalcFrame = Device.dwFrame;
	iAmmoCurrent = 0;

	for(int i = 0; i < (int)m_ammoTypes.size(); ++i) 
	{
		LPCSTR l_ammoType = *m_ammoTypes[i];

		for(TIItemContainer::iterator l_it = m_pInventory->m_belt.begin(); m_pInventory->m_belt.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = dynamic_cast<CWeaponAmmo*>(*l_it);

			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}

		for(TIItemContainer::iterator l_it = m_pInventory->m_ruck.begin(); m_pInventory->m_ruck.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = dynamic_cast<CWeaponAmmo*>(*l_it);
			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}

		if (!use_item_to_spawn)
			continue;

		if (!inventory_owner().item_to_spawn())
			continue;

		iAmmoCurrent += inventory_owner().ammo_in_box_to_spawn();
	}
	return l_count + iAmmoCurrent;
}

int CWeapon::GetCurrentTypeAmmoTotal() const
{
	int l_count = iAmmoElapsed;
	if ( !m_pInventory )
	{
		return l_count;
	}

	//чтоб не делать лишних пересчетов
	if ( m_pInventory->ModifyFrame() <= m_dwAmmoCurrentCalcFrame )
	{
		return l_count + iAmmoCurrent;
	}

	m_dwAmmoCurrentCalcFrame = Device.dwFrame;
	iAmmoCurrent = 0;

	VERIFY( 0 <= m_ammoType && m_ammoType < m_ammoTypes.size() );
	{
		LPCSTR l_ammoType = m_ammoTypes[m_ammoType].c_str();

		for(TIItemContainer::iterator l_it = m_pInventory->m_belt.begin(); m_pInventory->m_belt.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = dynamic_cast<CWeaponAmmo*>(*l_it);

			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}

		for(TIItemContainer::iterator l_it = m_pInventory->m_ruck.begin(); m_pInventory->m_ruck.end() != l_it; ++l_it) 
		{
			CWeaponAmmo *l_pAmmo = dynamic_cast<CWeaponAmmo*>(*l_it);
			if(l_pAmmo && !xr_strcmp(l_pAmmo->cNameSect(), l_ammoType)) 
			{
				iAmmoCurrent = iAmmoCurrent + l_pAmmo->m_boxCurr;
			}
		}
	}
	return l_count + iAmmoCurrent;
}

float CWeapon::GetConditionMisfireProbability() const
{
	if(GetCondition() > misfireStartCondition) 
		return 0.0f;
	if(GetCondition() < misfireEndCondition) 
		return misfireEndProbability;
	float mis = misfireStartProbability + (
		(misfireStartCondition - GetCondition()) *				// condition goes from 1.f to 0.f
		(misfireEndProbability - misfireStartProbability) /		// probability goes from 0.f to 1.f
		((misfireStartCondition == misfireEndCondition) ?		// !!!say "No" to devision by zero
			misfireStartCondition : 
			(misfireStartCondition - misfireEndCondition))
										  );
	clamp(mis,0.0f,0.99f);
	return mis;
}

BOOL CWeapon::CheckForMisfire()
{
	float rnd = ::Random.randF(0.f,1.f);
	float mp = GetConditionMisfireProbability();
	if(rnd < mp)
	{
		FireEnd();

		bMisfire = true;
		SwitchState(eMisfire);		
		
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL CWeapon::IsMisfire() const
{	
	return bMisfire;
}

float CWeapon::GetConditionLightMisfireProbability() const
{
	if(GetCondition() > l_misfireStartCondition) 
		return 0.0f;
	if(GetCondition() < l_misfireEndCondition) 
		return l_misfireEndProbability;
	float l_mis = l_misfireStartProbability + (
		(l_misfireStartCondition - GetCondition()) *				// condition goes from 1.f to 0.f
		(l_misfireEndProbability - l_misfireStartProbability) /		// probability goes from 0.f to 1.f
		((l_misfireStartCondition == l_misfireEndCondition) ?		// !!!say "No" to devision by zero
			l_misfireStartCondition : 
			(l_misfireStartCondition - l_misfireEndCondition))
		);
	clamp(l_mis, 0.0f, 0.99f);
	return l_mis;
}

BOOL CWeapon::CheckForLightMisfire()
{
	if(!m_bUseLightMisfire)
		return FALSE;

    if (l_misfireStartCondition > misfireStartCondition)
		return FALSE;

	float rnd = ::Random.randF(0.f,1.f);
	float l_mp = GetConditionLightMisfireProbability();

	if(rnd < l_mp)
		return TRUE;
	else
		return FALSE;
}

void CWeapon::Reload()
{}


bool CWeapon::IsGrenadeLauncherAttached() const
{
	return (ALife::eAddonAttachable == m_eGrenadeLauncherStatus &&
			0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonGrenadeLauncher)) || 
			ALife::eAddonPermanent == m_eGrenadeLauncherStatus;
}

bool CWeapon::IsScopeAttached() const
{
	return (ALife::eAddonAttachable == m_eScopeStatus &&
			0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonScope)) || 
			ALife::eAddonPermanent == m_eScopeStatus;

}

bool CWeapon::IsSilencerAttached() const
{
	return (ALife::eAddonAttachable == m_eSilencerStatus &&
			0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonSilencer)) || 
			ALife::eAddonPermanent == m_eSilencerStatus;
}

bool CWeapon::IsHandlerAttached() const
{
	return (ALife::eAddonAttachable == m_eHandlerStatus &&
			0 != (m_flagsAddOnState&CSE_ALifeItemWeapon::eWeaponAddonHandler)) || 
			ALife::eAddonPermanent == m_eHandlerStatus;
}

bool CWeapon::GrenadeLauncherAttachable()
{
	return (ALife::eAddonAttachable == m_eGrenadeLauncherStatus);
}
bool CWeapon::ScopeAttachable()
{
	return (ALife::eAddonAttachable == m_eScopeStatus);
}
bool CWeapon::SilencerAttachable()
{
	return (ALife::eAddonAttachable == m_eSilencerStatus);
}

bool CWeapon::HandlerAttachable()
{
	return (ALife::eAddonAttachable == m_eHandlerStatus);
}

shared_str wpn_scope				= "wpn_scope";
shared_str wpn_silencer				= "wpn_silencer";
shared_str wpn_grenade_launcher		= "wpn_launcher";

void CWeapon::UpdateHUDAddonsVisibility()
{//actor only
	if(!GetHUDmode())
		return;

	auto SetBoneVisible = [&](const shared_str& boneName, BOOL visibility)
	{
		HudItemData()->set_bone_visible(boneName, visibility, TRUE);
	};
	
	for (const shared_str& bone : m_defHiddenBones)
	{
		SetBoneVisible(bone, FALSE);
	}
	
	for (const shared_str& bone : m_defShownBones)
	{
		SetBoneVisible(bone, TRUE);
	}

	for (const shared_str& bone : m_upgShowBones)
	{
		SetBoneVisible(bone, TRUE);
	}

	for (const shared_str& bone : m_upgHideBones)
	{
		SetBoneVisible(bone, FALSE);
	}

	if (IsGrenadeLauncherAttached())
	{
		for (const shared_str& bone : m_defGLHiddenBones)
		{
			SetBoneVisible(bone, FALSE);
		}
	}

	if (IsSilencerAttached())
	{
		for (const shared_str& bone : m_defSilHiddenBones)
		{
			SetBoneVisible(bone, FALSE);
		}
	}

	if (IsZoomed())
	{
		if(bAltOffset && m_bHideMarkInAlt)
		{
			for (const shared_str& bone : m_colimSightBones)
			{
				SetBoneVisible(bone, FALSE);
			}
		}
		else
		{
			for (const shared_str& bone : m_colimSightBones)
			{
				SetBoneVisible(bone, TRUE);
			}
		}
	}
	else
	{
		for (const shared_str& bone : m_colimSightBones)
		{
			SetBoneVisible(bone, FALSE);
		}
	}

	if (m_sHud_wpn_flashlight_bone.size() && has_flashlight)
	{
		HudItemData()->set_bone_visible(m_sHud_wpn_flashlight_bone, IsFlashlightOn(), TRUE);
	}

	if(ScopeAttachable())
	{
		HudItemData()->set_bone_visible(wpn_scope, IsScopeAttached() );
	}

	if(m_eScopeStatus==ALife::eAddonDisabled )
	{
		HudItemData()->set_bone_visible(wpn_scope, FALSE, TRUE );
	}else
		if(m_eScopeStatus==ALife::eAddonPermanent)
			HudItemData()->set_bone_visible(wpn_scope, TRUE, TRUE );

	if(SilencerAttachable())
	{
		HudItemData()->set_bone_visible(wpn_silencer, IsSilencerAttached());
	}
	if(m_eSilencerStatus==ALife::eAddonDisabled )
	{
		HudItemData()->set_bone_visible(wpn_silencer, FALSE, TRUE);
	}
	else
		if(m_eSilencerStatus==ALife::eAddonPermanent)
			HudItemData()->set_bone_visible(wpn_silencer, TRUE, TRUE);

	if(GrenadeLauncherAttachable())
	{
		HudItemData()->set_bone_visible(wpn_grenade_launcher, IsGrenadeLauncherAttached());
	}
	if(m_eGrenadeLauncherStatus==ALife::eAddonDisabled )
	{
		HudItemData()->set_bone_visible(wpn_grenade_launcher, FALSE, TRUE);
	}
	else
		if(m_eGrenadeLauncherStatus==ALife::eAddonPermanent)
			HudItemData()->set_bone_visible(wpn_grenade_launcher, TRUE, TRUE);

	if(HandlerAttachable())
	{
		HudItemData()->set_bone_visible(m_sHandler_bone, IsHandlerAttached());
	}
	if(m_eHandlerStatus==ALife::eAddonDisabled )
	{
		HudItemData()->set_bone_visible(m_sHandler_bone, FALSE, TRUE);
	}
	else
		if(m_eHandlerStatus==ALife::eAddonPermanent)
			HudItemData()->set_bone_visible(m_sHandler_bone, TRUE, TRUE);
}

void CWeapon::UpdateAddonsVisibility()
{
	IKinematics* pWeaponVisual = dynamic_cast<IKinematics*>(Visual()); R_ASSERT(pWeaponVisual);

	u16  bone_id;
	UpdateHUDAddonsVisibility								();	

	pWeaponVisual->CalculateBones_Invalidate				();

	for (const auto& boneName : m_defHiddenBones)
	{
		bone_id = pWeaponVisual->LL_BoneID(boneName);
		if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
			pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
	}

	for (const auto& boneName : m_defShownBones)
	{
		bone_id = pWeaponVisual->LL_BoneID(boneName);
		if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
			pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
	}

	for (const auto& boneName : m_upgHideBones)
	{
		bone_id = pWeaponVisual->LL_BoneID(boneName);
		if (bone_id != BI_NONE)
			pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
	}

	for (const auto& boneName : m_upgShowBones)
	{
		bone_id = pWeaponVisual->LL_BoneID(boneName);
		if (bone_id != BI_NONE)
			pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
	}

	if (IsGrenadeLauncherAttached())
	{
		for (const auto& boneName : m_defGLHiddenBones)
		{
			bone_id = pWeaponVisual->LL_BoneID(boneName);
			if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
		}
	}

	if (IsSilencerAttached())
	{
		for (const auto& boneName : m_defSilHiddenBones)
		{
			bone_id = pWeaponVisual->LL_BoneID(boneName);
			if (bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
		}
	}

	bone_id = pWeaponVisual->LL_BoneID(wpn_scope);
	if(ScopeAttachable())
	{
		if(IsScopeAttached())
		{
			if(!pWeaponVisual->LL_GetBoneVisible(bone_id))
			pWeaponVisual->LL_SetBoneVisible(bone_id,TRUE,TRUE);
		}
		else
		{
			if(pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id,FALSE,TRUE);
		}
	}

	if(m_eScopeStatus == ALife::eAddonDisabled && bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
	{
		pWeaponVisual->LL_SetBoneVisible(bone_id,FALSE,TRUE);
	}

	bone_id = pWeaponVisual->LL_BoneID(wpn_silencer);
	if(SilencerAttachable())
	{
		if(IsSilencerAttached())
		{
			if(!pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id,TRUE,TRUE);
		}
		else
		{
			if(pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id,FALSE,TRUE);
		}
	}
	if(m_eSilencerStatus == ALife::eAddonDisabled && bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
	{
		pWeaponVisual->LL_SetBoneVisible					(bone_id,FALSE,TRUE);
	}

	///////////////////////////////////////////////////////////////////

	if (m_sWpn_flashlight_bone.size() && has_flashlight)
	{
		bone_id = pWeaponVisual->LL_BoneID(m_sWpn_flashlight_bone);

		if (bone_id != BI_NONE)
		{
			const bool flashlight_on = IsFlashlightOn();
			if (pWeaponVisual->LL_GetBoneVisible(bone_id) && !flashlight_on)
				pWeaponVisual->LL_SetBoneVisible(bone_id, FALSE, TRUE);
			else if (!pWeaponVisual->LL_GetBoneVisible(bone_id) && flashlight_on)
				pWeaponVisual->LL_SetBoneVisible(bone_id, TRUE, TRUE);
		}
	}

	bone_id = pWeaponVisual->LL_BoneID						(wpn_grenade_launcher);
	if(GrenadeLauncherAttachable())
	{
		if(IsGrenadeLauncherAttached())
		{
			if(!pWeaponVisual->LL_GetBoneVisible		(bone_id))
				pWeaponVisual->LL_SetBoneVisible			(bone_id,TRUE,TRUE);
		}
		else
		{
			if(pWeaponVisual->LL_GetBoneVisible				(bone_id))
				pWeaponVisual->LL_SetBoneVisible			(bone_id,FALSE,TRUE);
		}
	}
	if(m_eGrenadeLauncherStatus==ALife::eAddonDisabled && bone_id!=BI_NONE && 
		pWeaponVisual->LL_GetBoneVisible(bone_id) )
	{
		pWeaponVisual->LL_SetBoneVisible					(bone_id,FALSE,TRUE);
	}

	bone_id = pWeaponVisual->LL_BoneID(m_sHandler_bone);
	if(HandlerAttachable())
	{
		if(IsHandlerAttached())
		{
			if(!pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id,TRUE,TRUE);
		}
		else
		{
			if(pWeaponVisual->LL_GetBoneVisible(bone_id))
				pWeaponVisual->LL_SetBoneVisible(bone_id,FALSE,TRUE);
		}
	}
	if(m_eHandlerStatus == ALife::eAddonDisabled && bone_id != BI_NONE && pWeaponVisual->LL_GetBoneVisible(bone_id))
	{
		pWeaponVisual->LL_SetBoneVisible					(bone_id,FALSE,TRUE);
	}

	///////////////////////////////////////////////////////////////////

	pWeaponVisual->CalculateBones_Invalidate				();
	pWeaponVisual->CalculateBones							(TRUE);
}

void CWeapon::InitAddons()
{
}

float CWeapon::CurrentZoomFactor()
{
	return IsScopeAttached() ? m_zoom_params.m_fScopeZoomFactor : m_zoom_params.m_fIronSightZoomFactor;
};
void GetZoomData(const float scope_factor, float& delta, float& min_zoom_factor);
void CWeapon::OnZoomIn()
{
	m_zoom_params.m_bIsZoomModeNow		= true;
	if(m_bUseDynamicZoom)
		SetZoomFactor(m_fRTZoomFactor);
	else
		m_zoom_params.m_fCurrentZoomFactor	= CurrentZoomFactor();

    // Отключаем инерцию (Заменено GetInertionFactor())
    // EnableHudInertion(FALSE);
	
	if(m_zoom_params.m_bZoomDofEnabled && !IsScopeAttached())
		GamePersistent().SetEffectorDOF	(m_zoom_params.m_ZoomDof);
}

void CWeapon::OnZoomOut()
{
	m_zoom_params.m_bIsZoomModeNow		= false;
	m_zoom_params.m_fCurrentZoomFactor	= g_fov;

    // Включаем инерцию (также заменено  GetInertionFactor())
    // EnableHudInertion	(TRUE);

 	GamePersistent().RestoreEffectorDOF	();
	ResetSubStateTime					();
}

CUIWindow* CWeapon::ZoomTexture()
{
	if (UseScopeTexture() && !bAltOffset)
		return m_UIScope;
	else
		return NULL;
}

void CWeapon::SwitchState(u32 S)
{
	if (OnClient()) return;

#ifndef MASTER_GOLD
	if ( bDebug )
	{
		Msg("---Server is going to send GE_WPN_STATE_CHANGE to [%d], weapon_section[%s], parent[%s]",
			S, cNameSect().c_str(), H_Parent() ? H_Parent()->cName().c_str() : "NULL Parent");
	}
#endif // #ifndef MASTER_GOLD

	SetNextState( S );
	if (CHudItem::object().Local() && !CHudItem::object().getDestroy() && m_pInventory && OnServer())	
	{
		// !!! Just single entry for given state !!!
		NET_Packet P;
		CHudItem::object().u_EventGen(P,GE_WPN_STATE_CHANGE,CHudItem::object().ID());
		P.w_u8(u8(S));
		P.w_u8(u8(m_sub_state));
		P.w_u8(u8(m_ammoType& 0xff));
		P.w_u8(u8(iAmmoElapsed & 0xff));
		P.w_u8(u8(m_set_next_ammoType_on_reload & 0xff));
		CHudItem::object().u_EventSend(P, net_flags(TRUE, TRUE, FALSE, TRUE));
	}
}

void CWeapon::OnMagazineEmpty()
{
	VERIFY((u32)iAmmoElapsed == m_magazine.size());
}

void CWeapon::reinit()
{
	CShootingObject::reinit();
	CHudItemObject::reinit();
}

void CWeapon::reload			(LPCSTR section)
{
	CShootingObject::reload		(section);
	CHudItemObject::reload			(section);
	
	m_can_be_strapped			= true;
	m_strapped_mode				= false;
	
	if (pSettings->line_exist(section,"strap_bone0"))
		m_strap_bone0			= pSettings->r_string(section,"strap_bone0");
	else
		m_can_be_strapped		= false;
	
	if (pSettings->line_exist(section,"strap_bone1"))
		m_strap_bone1			= pSettings->r_string(section,"strap_bone1");
	else
		m_can_be_strapped		= false;

	if (m_eScopeStatus == ALife::eAddonAttachable) {
		m_addon_holder_range_modifier	= READ_IF_EXISTS(pSettings,r_float,m_sScopeName,"holder_range_modifier",m_holder_range_modifier);
		m_addon_holder_fov_modifier		= READ_IF_EXISTS(pSettings,r_float,m_sScopeName,"holder_fov_modifier",m_holder_fov_modifier);
	}
	else {
		m_addon_holder_range_modifier	= m_holder_range_modifier;
		m_addon_holder_fov_modifier		= m_holder_fov_modifier;
	}


	{
		Fvector				pos,ypr;
		pos					= pSettings->r_fvector3		(section,"position");
		ypr					= pSettings->r_fvector3		(section,"orientation");
		ypr.mul				(PI/180.f);

		m_Offset.setHPB			(ypr.x,ypr.y,ypr.z);
		m_Offset.translate_over	(pos);
	}

	m_StrapOffset			= m_Offset;
	if (pSettings->line_exist(section,"strap_position") && pSettings->line_exist(section,"strap_orientation")) {
		Fvector				pos,ypr;
		pos					= pSettings->r_fvector3		(section,"strap_position");
		ypr					= pSettings->r_fvector3		(section,"strap_orientation");
		ypr.mul				(PI/180.f);

		m_StrapOffset.setHPB			(ypr.x,ypr.y,ypr.z);
		m_StrapOffset.translate_over	(pos);
	}
	else
		m_can_be_strapped	= false;

	m_ef_main_weapon_type	= READ_IF_EXISTS(pSettings,r_u32,section,"ef_main_weapon_type",u32(-1));
	m_ef_weapon_type		= READ_IF_EXISTS(pSettings,r_u32,section,"ef_weapon_type",u32(-1));
}

void CWeapon::create_physic_shell()
{
	CPhysicsShellHolder::create_physic_shell();
}

void CWeapon::activate_physic_shell()
{
	UpdateXForm();
	CPhysicsShellHolder::activate_physic_shell();
}

void CWeapon::setup_physic_shell()
{
	CPhysicsShellHolder::setup_physic_shell();
}

int		g_iWeaponRemove = 1;

bool CWeapon::NeedToDestroyObject()	const
{
	if (GameID() == eGameIDSingle) return false;
	if (Remote()) return false;
	if (H_Parent()) return false;
	if (g_iWeaponRemove == -1) return false;
	if (g_iWeaponRemove == 0) return true;
	if (TimePassedAfterIndependant() > m_dwWeaponRemoveTime)
		return true;

	return false;
}

ALife::_TIME_ID	 CWeapon::TimePassedAfterIndependant()	const
{
	if(!H_Parent() && m_dwWeaponIndependencyTime != 0)
		return Level().timeServer() - m_dwWeaponIndependencyTime;
	else
		return 0;
}

bool CWeapon::can_kill	() const
{
	if (GetSuitableAmmoTotal(true) || m_ammoTypes.empty())
		return				(true);

	return					(false);
}

CInventoryItem *CWeapon::can_kill	(CInventory *inventory) const
{
	if (GetAmmoElapsed() || m_ammoTypes.empty())
		return				(const_cast<CWeapon*>(this));

	TIItemContainer::iterator I = inventory->m_all.begin();
	TIItemContainer::iterator E = inventory->m_all.end();
	for ( ; I != E; ++I) {
		CInventoryItem	*inventory_item = dynamic_cast<CInventoryItem*>(*I);
		if (!inventory_item)
			continue;
		
		xr_vector<shared_str>::const_iterator	i = std::find(m_ammoTypes.begin(),m_ammoTypes.end(),inventory_item->object().cNameSect());
		if (i != m_ammoTypes.end())
			return			(inventory_item);
	}

	return					(0);
}

const CInventoryItem *CWeapon::can_kill	(const xr_vector<const CGameObject*> &items) const
{
	if (m_ammoTypes.empty())
		return				(this);

	xr_vector<const CGameObject*>::const_iterator I = items.begin();
	xr_vector<const CGameObject*>::const_iterator E = items.end();
	for ( ; I != E; ++I) {
		const CInventoryItem	*inventory_item = dynamic_cast<const CInventoryItem*>(*I);
		if (!inventory_item)
			continue;

		xr_vector<shared_str>::const_iterator	i = std::find(m_ammoTypes.begin(),m_ammoTypes.end(),inventory_item->object().cNameSect());
		if (i != m_ammoTypes.end())
			return			(inventory_item);
	}

	return					(0);
}

bool CWeapon::ready_to_kill	() const
{
	return					(
		!IsMisfire() && 
		((GetState() == eIdle) || (GetState() == eFire) || (GetState() == eFire2)) && 
		GetAmmoElapsed()
	);
}

u8 CWeapon::GetCurrentHudOffsetIdx()
{
    auto pActor = dynamic_cast<CActor*>(H_Parent());
    if (!pActor)
        return 0;

    bool b_aiming = ((IsZoomed() && m_zoom_params.m_fZoomRotationFactor <= 1.f) || (!IsZoomed() && m_zoom_params.m_fZoomRotationFactor > 0.f));
	bool b_safemode = ((pActor->IsSafemode() && m_fSafemodeRotationFactor <= 1.f) || (!pActor->IsSafemode() && m_fSafemodeRotationFactor > 0.f));

	if (m_bCanBeLowered && b_safemode)
        return 4;
    else if (!b_aiming)
        return 0;
    else if (bAltOffset)
        return 3;
    else
        return 1;
}

void CWeapon::UpdateHudAdditional(Fmatrix& trans)
{
    auto pActor = dynamic_cast<const CActor*>(H_Parent());
	if(!pActor)
		return;

	auto idx = GetCurrentHudOffsetIdx();

	bool b_safemode = ((pActor->IsSafemode() && m_fSafemodeRotationFactor <= 1.f) || (!pActor->IsSafemode() && m_fSafemodeRotationFactor > 0.f));

	attachable_hud_item*		hi = HudItemData();
	R_ASSERT					(hi);
	Fvector						curr_offs, curr_rot;
	curr_offs = hi->m_measures.m_hands_offset[0][idx];//pos,aim
	curr_rot = hi->m_measures.m_hands_offset[1][idx];//rot,aim
	curr_offs.mul				(b_safemode ? m_fSafemodeRotationFactor : m_zoom_params.m_fZoomRotationFactor);
	curr_rot.mul				(b_safemode ? m_fSafemodeRotationFactor : m_zoom_params.m_fZoomRotationFactor);

	Fmatrix						hud_rotation;
	hud_rotation.identity		();
	hud_rotation.rotateX		(curr_rot.x);

	Fmatrix						hud_rotation_y;
	hud_rotation_y.identity		();
	hud_rotation_y.rotateY		(curr_rot.y);
	hud_rotation.mulA_43		(hud_rotation_y);

	hud_rotation_y.identity		();
	hud_rotation_y.rotateZ		(curr_rot.z);
	hud_rotation.mulA_43		(hud_rotation_y);

	hud_rotation.translate_over	(curr_offs);
	trans.mulB_43				(hud_rotation);

	if(pActor->IsZoomAimingMode())
		m_zoom_params.m_fZoomRotationFactor += Device.fTimeDelta/m_zoom_params.m_fZoomRotateTime;
	else
		m_zoom_params.m_fZoomRotationFactor -= Device.fTimeDelta/m_zoom_params.m_fZoomRotateTime;

	clamp(m_zoom_params.m_fZoomRotationFactor, 0.f, 1.f);

	if(pActor->IsSafemode())
		m_fSafemodeRotationFactor += Device.fTimeDelta/m_fSafemodeRotateTime;
	else
		m_fSafemodeRotationFactor -= Device.fTimeDelta/m_fSafemodeRotateTime;

	clamp(m_fSafemodeRotationFactor, 0.f, 1.f);

	// Боковой стрейф с оружием
	clamp(idx, 0ui8, 1ui8);

	// Рассчитываем фактор боковой ходьбы
	float fStrafeMaxTime = /*hi->m_measures.*/m_strafe_offset[2][idx].y; // Макс. время в секундах, за которое мы наклонимся из центрального положения
	if (fStrafeMaxTime <= EPS)
		fStrafeMaxTime = 0.01f;

	float fStepPerUpd = Device.fTimeDelta / fStrafeMaxTime; // Величина изменение фактора поворота

	u32 iMovingState = pActor->MovingState();
	if ((iMovingState & mcLStrafe) != 0)
	{ // Движемся влево
		float fVal = (m_fLR_MovingFactor > 0.f ? fStepPerUpd * 3 : fStepPerUpd);
		m_fLR_MovingFactor -= fVal;
	}
	else if ((iMovingState & mcRStrafe) != 0)
	{ // Движемся вправо
		float fVal = (m_fLR_MovingFactor < 0.f ? fStepPerUpd * 3 : fStepPerUpd);
		m_fLR_MovingFactor += fVal;
	}
	else
	{ // Двигаемся в любом другом направлении
		if (m_fLR_MovingFactor < 0.0f)
		{
			m_fLR_MovingFactor += fStepPerUpd;
			clamp(m_fLR_MovingFactor, -1.0f, 0.0f);
		}
		else
		{
			m_fLR_MovingFactor -= fStepPerUpd;
			clamp(m_fLR_MovingFactor, 0.0f, 1.0f);
		}
	}

	clamp(m_fLR_MovingFactor, -1.0f, 1.0f); // Фактор боковой ходьбы не должен превышать эти лимиты

	// Производим наклон ствола для нормального режима и аима
	for (int _idx = 0; _idx <= 1; _idx++)
	{
		bool bEnabled = /*hi->m_measures.*/m_strafe_offset[2][_idx].x;
		if (!bEnabled)
			continue;

		Fvector curr_offs, curr_rot;

		// Смещение позиции худа в стрейфе
		curr_offs = /*hi->m_measures.*/m_strafe_offset[0][_idx]; //pos
		curr_offs.mul(m_fLR_MovingFactor);                   // Умножаем на фактор стрейфа

		// Поворот худа в стрейфе
		curr_rot = /*hi->m_measures.*/m_strafe_offset[1][_idx]; //rot
		curr_rot.mul(-PI / 180.f);                          // Преобразуем углы в радианы
		curr_rot.mul(m_fLR_MovingFactor);                   // Умножаем на фактор стрейфа

		if (_idx == 0)
		{ // От бедра
			curr_offs.mul(1.f - m_zoom_params.m_fZoomRotationFactor);
			curr_rot.mul(1.f - m_zoom_params.m_fZoomRotationFactor);
		}
		else
		{ // Во время аима
			curr_offs.mul(m_zoom_params.m_fZoomRotationFactor);
			curr_rot.mul(m_zoom_params.m_fZoomRotationFactor);
		}

		Fmatrix hud_rotation;
		Fmatrix hud_rotation_y;

		hud_rotation.identity();
		hud_rotation.rotateX(curr_rot.x);

		hud_rotation_y.identity();
		hud_rotation_y.rotateY(curr_rot.y);
		hud_rotation.mulA_43(hud_rotation_y);

		hud_rotation_y.identity();
		hud_rotation_y.rotateZ(curr_rot.z);
		hud_rotation.mulA_43(hud_rotation_y);

		hud_rotation.translate_over(curr_offs);
		trans.mulB_43(hud_rotation);
	}
}

void CWeapon::SetAmmoElapsed(int ammo_count)
{
	iAmmoElapsed				= ammo_count;

	u32 uAmmo					= u32(iAmmoElapsed);

	if (uAmmo != m_magazine.size())
	{
		if (uAmmo > m_magazine.size())
		{
			CCartridge			l_cartridge; 
			l_cartridge.Load	(*m_ammoTypes[m_ammoType], u8(m_ammoType));
			while (uAmmo > m_magazine.size())
				m_magazine.push_back(l_cartridge);
		}
		else
		{
			while (uAmmo < m_magazine.size())
				m_magazine.pop_back();
		};
	};
}

u32	CWeapon::ef_main_weapon_type	() const
{
	VERIFY	(m_ef_main_weapon_type != u32(-1));
	return	(m_ef_main_weapon_type);
}

u32	CWeapon::ef_weapon_type	() const
{
	VERIFY	(m_ef_weapon_type != u32(-1));
	return	(m_ef_weapon_type);
}

bool CWeapon::IsNecessaryItem	    (const shared_str& item_sect)
{
	return (std::find(m_ammoTypes.begin(), m_ammoTypes.end(), item_sect) != m_ammoTypes.end() );
}

void CWeapon::modify_holder_params		(float &range, float &fov) const
{
	if (!IsScopeAttached()) {
		inherited::modify_holder_params	(range,fov);
		return;
	}
	range	*= m_addon_holder_range_modifier;
	fov		*= m_addon_holder_fov_modifier;
}

bool CWeapon::render_item_ui_query()
{
	bool b_is_active_item = (m_pInventory->ActiveItem()==this);
	bool res = b_is_active_item && IsZoomed() && ZoomHideCrosshair() && ZoomTexture() && !IsRotatingToZoom();
	return res;
}

void CWeapon::render_item_ui()
{
	ZoomTexture()->Update	();
	ZoomTexture()->Draw		();
}

bool CWeapon::unlimited_ammo() 
{ 
	if (IsGameTypeSingle())
		return psActorFlags.test(AF_UNLIMITEDAMMO) && 
				m_DefaultCartridge.m_flags.test(CCartridge::cfCanBeUnlimited); 

	return ((GameID() != eGameIDArtefactHunt) && 
			(GameID() != eGameIDCaptureTheArtefact) &&
			m_DefaultCartridge.m_flags.test(CCartridge::cfCanBeUnlimited)); 
			
};

LPCSTR	CWeapon::GetCurrentAmmo_ShortName	()
{
	if (m_magazine.empty()) return ("");
	CCartridge &l_cartridge = m_magazine.back();
	return *(l_cartridge.m_InvShortName);
}

float CWeapon::Weight() const
{
	float res = CInventoryItemObject::Weight();
	if(IsGrenadeLauncherAttached()&&GetGrenadeLauncherName().size()){
		res += pSettings->r_float(GetGrenadeLauncherName(),"inv_weight");
	}
	if(IsScopeAttached()&&GetScopeName().size()){
		res += pSettings->r_float(GetScopeName(),"inv_weight");
	}
	if(IsSilencerAttached()&&GetSilencerName().size()){
		res += pSettings->r_float(GetSilencerName(),"inv_weight");
	}
	
	if(iAmmoElapsed)
	{
		float w		= pSettings->r_float(*m_ammoTypes[m_ammoType],"inv_weight");
		float bs	= pSettings->r_float(*m_ammoTypes[m_ammoType],"box_size");

		res			+= w*(iAmmoElapsed/bs);
	}
	return res;
}

u32 CWeapon::Cost() const
{
	u32 res = CInventoryItem::Cost();
	if(IsGrenadeLauncherAttached()&&GetGrenadeLauncherName().size())
	{
		res += pSettings->r_u32(GetGrenadeLauncherName(),"cost");
	}
	if(IsScopeAttached()&&GetScopeName().size())
	{
		res += pSettings->r_u32(GetScopeName(),"cost");
	}
	if(IsSilencerAttached()&&GetSilencerName().size())
	{
		res += pSettings->r_u32(GetSilencerName(),"cost");
	}
	
	if(iAmmoElapsed)
	{
		float w		= pSettings->r_float(m_ammoTypes[m_ammoType].c_str(),"cost");
		float bs	= pSettings->r_float(m_ammoTypes[m_ammoType].c_str(),"box_size");

		res			+= iFloor(w*(iAmmoElapsed /bs));
	}
	return res;
}

extern bool hud_adj_crosshair;

bool CWeapon::show_crosshair()
{
	return (StatesNoHideCrosshair() && ( !IsZoomed() || !ZoomHideCrosshair())) || (hud_adj_mode != 0 && hud_adj_crosshair);
}

bool CWeapon::show_indicators()
{
	return ! ( IsZoomed() && ZoomTexture() );
}

float CWeapon::GetConditionToShow	() const
{
	return	(GetCondition());//powf(GetCondition(),4.0f));
}

BOOL CWeapon::ParentMayHaveAimBullet	()
{
	CObject* O=H_Parent();
	CEntityAlive* EA=dynamic_cast<CEntityAlive*>(O);
	return EA->cast_actor()!=0;
}

void CWeapon::debug_draw_firedeps()
{
#ifdef DEBUG
	if(hud_adj_mode==5||hud_adj_mode==6||hud_adj_mode==7)
	{
		CDebugRenderer			&render = Level().debug_renderer();

		if(hud_adj_mode==5)
			render.draw_aabb(get_LastFP(),	0.005f,0.005f,0.005f,D3DCOLOR_XRGB(255,0,0));

		if(hud_adj_mode==6)
			render.draw_aabb(get_LastFP2(),	0.005f,0.005f,0.005f,D3DCOLOR_XRGB(0,0,255));

		if(hud_adj_mode==7)
			render.draw_aabb(get_LastSP(),		0.005f,0.005f,0.005f,D3DCOLOR_XRGB(0,255,0));
	}
#endif // DEBUG
}

const float &CWeapon::hit_probability	() const
{
	VERIFY					((g_SingleGameDifficulty >= egdNovice) && (g_SingleGameDifficulty <= egdMaster)); 
	return					(m_hit_probability[egdNovice]);
}

bool CWeapon::NoSprintStates()
{
	return (GetState() == eIdle || GetState() == eMisfire || GetState() == eHidden || GetState() == eSprintStart);
}

bool CWeapon::StatesNoHideCrosshair()
{
	if(!IsPending())
		return true;
	else
		return IsPending() && (GetState() == eSprintStart || GetState() == eSprintEnd || GetState() == eLookMis || GetState() == eUnLightMis || GetState() == eHideDet || GetState() == eShowingDet || GetState() == eShowingEndDet);
}

void CWeapon::OnStateSwitch	(u32 S)
{
	inherited::OnStateSwitch(S);
	m_dwAmmoCurrentCalcFrame = 0;

	auto current_actor = dynamic_cast<CActor*>(H_Parent());

	if(GetState() == eReload || GetState() == eLookMis && !IsZoomed())
	{
		if(H_Parent()==Level().CurrentEntity() && !fsimilar(m_zoom_params.m_ReloadDof.w,-1.0f))
		{
			if (current_actor)
				current_actor->Cameras().AddCamEffector(xr_new<CEffectorDOF>(m_zoom_params.m_ReloadDof) );
		}
	}

	if (GetState() == eHiding || GetState() == eShowing)
	{
		attachable_hud_item* i1 = g_player_hud->attached_item(1);
		if (i1 && HudItemData())
		{
			auto det = dynamic_cast<CCustomDetector*>(i1->m_parent_hud_item);
			if (det)
			{
				if (GetState() == eShowing)
					det->SwitchState(CCustomDetector::eShowingParItm);
				else
					det->SwitchState(CCustomDetector::eHideParItm);
			}
		}
	}

	if (current_actor && GetState() == eHiding && current_actor->IsSafemode())
		Actor()->SetSafemodeStatus(false);
}

void CWeapon::OnAnimationEnd(u32 state) 
{
	inherited::OnAnimationEnd(state);
}

void CWeapon::render_hud_mode()
{
	RenderLight();
}

bool CWeapon::MovingAnimAllowedNow()
{
	return !IsZoomed();
}

bool CWeapon::IsHudModeNow()
{
	return (HudItemData()!=NULL);
}

// Получить FOV от текущего оружия игрока для второго рендера
float CWeapon::GetSecondVPFov() const
{
	if (m_bUseDynamicZoom && IsSecondVPZoomPresent())
		return (m_fRTZoomFactor / 100.f) * g_fov;

	return GetSecondVPZoomFactor() * g_fov;
}

// Обновление необходимости включения второго вьюпорта +SecondVP+
// Вызывается только для активного оружия игрока
void CWeapon::UpdateSecondVP()
{
	bool b_is_active_item = (m_pInventory != NULL) && (m_pInventory->ActiveItem() == this);
	R_ASSERT(ParentIsActor() && b_is_active_item); // Эта функция должна вызываться только для оружия в руках нашего игрока

	CActor* pActor = dynamic_cast<CActor*>(H_Parent());
	
	bool bCond_1 = m_zoom_params.m_fZoomRotationFactor > 0.05f;							// Мы должны целиться

	bool bCond_2 = IsSecondVPZoomPresent()/* && psActorFlags.test(AF_3DSCOPE_ENABLE)*/;	// В конфиге должен быть прописан фактор зума для линзы (scope_lense_factor
																						// больше чем 0)
	bool bCond_3 = pActor->cam_Active() == pActor->cam_FirstEye();						// Мы должны быть от 1-го лица	

	Device.m_SecondViewport.SetSVPActive(bCond_1 && bCond_2 && bCond_3);
}

void CWeapon::ZoomInc()
{
	if(!IsScopeAttached())
		return;

	if (bAltOffset)
		return;

	if(!m_bUseDynamicZoom)
		return;

	float delta,min_zoom_factor;
	GetZoomData(m_zoom_params.m_fScopeZoomFactor, delta, min_zoom_factor);

	float f					= GetZoomFactor()-delta;
	clamp					(f,m_zoom_params.m_fScopeZoomFactor,min_zoom_factor);
	SetZoomFactor			( f );
}

void CWeapon::ZoomDec()
{
	if(!IsScopeAttached())
		return;

	if (bAltOffset)
		return;

	if(!m_bUseDynamicZoom)
		return;

	float delta,min_zoom_factor;
	GetZoomData(m_zoom_params.m_fScopeZoomFactor,delta,min_zoom_factor);

	float f					= GetZoomFactor()+delta;
	clamp					(f,m_zoom_params.m_fScopeZoomFactor,min_zoom_factor);
	SetZoomFactor			( f );

}

bool CWeapon::IsGrenadeLauncherMode()
{
    CWeaponMagazinedWGrenade* maggl = dynamic_cast<CWeaponMagazinedWGrenade*>(this);
    return !!(maggl && maggl->m_bGrenadeMode);
}