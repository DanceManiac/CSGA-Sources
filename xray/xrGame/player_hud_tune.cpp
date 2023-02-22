#include "stdafx.h"
#include "player_hud.h"
#include "level.h"
#include "debug_renderer.h"
#include "../xrEngine/xr_input.h"
#include "HudManager.h"
#include "HudItem.h"
#include "../xrEngine/Effector.h"
#include "../xrEngine/CameraManager.h"
#include "../xrEngine/FDemoRecord.h"
#include "debug_renderer.h"
#include "xr_level_controller.h"

bool hud_adj_active		= false; //Включение/выключение
u32 hud_adj_mode		= 7; //Режим
u32 hud_adj_item_idx	= 0; //Итем
u32 hud_adj_offset		= 0; //Тип вращения

float _delta_pos			= 0.0005f;
float _delta_rot			= 0.05f;

void tune_remap(const Ivector& in_values, Ivector& out_values)
{
	if( pInput->iGetAsyncKeyState(DIK_LSHIFT) )
	{
		out_values = in_values;
	}
	else if( pInput->iGetAsyncKeyState(DIK_Z) )
	{ //strict by X
		out_values.x = in_values.y;
		out_values.y = 0;
		out_values.z = 0;
	}else
	if( pInput->iGetAsyncKeyState(DIK_X) )
	{ //strict by Y
		out_values.x = 0;
		out_values.y = in_values.y;
		out_values.z = 0;
	}else
	if( pInput->iGetAsyncKeyState(DIK_C) )
	{ //strict by Z
		out_values.x = 0;
		out_values.y = 0;
		out_values.z = in_values.y;
	}else
	{
		out_values.set(0,0,0);
	}
}

void calc_cam_diff_pos(Fmatrix item_transform, Fvector diff, Fvector& res)
{
	Fmatrix							cam_m;
	cam_m.i.set						(Device.vCameraRight);
	cam_m.j.set						(Device.vCameraTop);
	cam_m.k.set						(Device.vCameraDirection);
	cam_m.c.set						(Device.vCameraPosition);


	Fvector							res1;
	cam_m.transform_dir				(res1, diff);

	Fmatrix							item_transform_i;
	item_transform_i.invert			(item_transform);
	item_transform_i.transform_dir	(res, res1);
}

void calc_cam_diff_rot(Fmatrix item_transform, Fvector diff, Fvector& res)
{
	Fmatrix							cam_m;
	cam_m.i.set						(Device.vCameraRight);
	cam_m.j.set						(Device.vCameraTop);
	cam_m.k.set						(Device.vCameraDirection);
	cam_m.c.set						(Device.vCameraPosition);

	Fmatrix							R;
	R.identity						();
	if(!fis_zero(diff.x))
	{
		R.rotation(cam_m.i,diff.x);
	}else
	if(!fis_zero(diff.y))
	{
		R.rotation(cam_m.j,diff.y);
	}else
	if(!fis_zero(diff.z))
	{
		R.rotation(cam_m.k,diff.z);
	};

	Fmatrix					item_transform_i;
	item_transform_i.invert	(item_transform);
	R.mulB_43(item_transform);
	R.mulA_43(item_transform_i);
	
	R.getHPB	(res);

	res.mul					(180.0f/PI);
}

void attachable_hud_item::tune(Ivector values)
{
#ifndef MASTER_GOLD

	Fvector					diff;
	diff.set				(0,0,0);

	if(hud_adj_mode == 2)
	{
		if (hud_adj_offset == 0)
		{
			if(values.x)	diff.x = (values.x>0)?_delta_pos:-_delta_pos;
			if(values.y)	diff.y = (values.y>0)?_delta_pos:-_delta_pos;
			if(values.z)	diff.z = (values.z>0)?_delta_pos:-_delta_pos;
			
			Fvector							d;
			Fmatrix							ancor_m;
			m_parent->calc_transform		(m_attach_place_idx, Fidentity, ancor_m);
			calc_cam_diff_pos				(ancor_m, diff, d);
			m_measures.m_item_attach[0].add	(d);
		}
		else if (hud_adj_offset == 1)
		{
			if(values.x)	diff.x = (values.x>0)?_delta_rot:-_delta_rot;
			if(values.y)	diff.y = (values.y>0)?_delta_rot:-_delta_rot;
			if(values.z)	diff.z = (values.z>0)?_delta_rot:-_delta_rot;

			Fvector							d;
			Fmatrix							ancor_m;
			m_parent->calc_transform		(m_attach_place_idx, Fidentity, ancor_m);

			calc_cam_diff_pos				(m_item_transform, diff, d);
			m_measures.m_item_attach[1].add	(d);
		}

		if((values.x)||(values.y)||(values.z))
		{
			Msg("[%s]",m_sect_name.c_str());
			Msg("item_position				= %f,%f,%f",m_measures.m_item_attach[0].x, m_measures.m_item_attach[0].y, m_measures.m_item_attach[0].z);
			Msg("item_orientation			= %f,%f,%f",m_measures.m_item_attach[1].x, m_measures.m_item_attach[1].y, m_measures.m_item_attach[1].z);
			Log("-----------");
		}
	}

	if(hud_adj_mode == 3 || hud_adj_mode == 4 || hud_adj_mode == 5)
	{
		if(values.x)	diff.x = (values.x>0)?_delta_pos:-_delta_pos;
		if(values.y)	diff.y = (values.y>0)?_delta_pos:-_delta_pos;
		if(values.z)	diff.z = (values.z>0)?_delta_pos:-_delta_pos;

		if(hud_adj_mode == 3)
		{
			m_measures.m_fire_point_offset.add(diff);
		}
		if(hud_adj_mode == 4)
		{
			m_measures.m_fire_point2_offset.add(diff);
		}
		if(hud_adj_mode == 5)
		{
			m_measures.m_shell_point_offset.add(diff);
		}
		if((values.x)||(values.y)||(values.z))
		{
			Msg("[%s]",					m_sect_name.c_str());
			Msg("fire_point				= %f,%f,%f",m_measures.m_fire_point_offset.x,	m_measures.m_fire_point_offset.y,	m_measures.m_fire_point_offset.z);
			Msg("fire_point2			= %f,%f,%f",m_measures.m_fire_point2_offset.x,	m_measures.m_fire_point2_offset.y,	m_measures.m_fire_point2_offset.z);
			Msg("shell_point			= %f,%f,%f",m_measures.m_shell_point_offset.x,	m_measures.m_shell_point_offset.y,	m_measures.m_shell_point_offset.z);
			Log("-----------");
		}
	}
#endif // #ifndef MASTER_GOLD
}

void attachable_hud_item::debug_draw_firedeps()
{
#ifdef DEBUG

	if (!hud_adj_active)
        return;

	bool bForce = (hud_adj_mode==3||hud_adj_mode==4);
    if (hud_adj_mode == 3 || hud_adj_mode == 4 || hud_adj_mode == 5 || bForce)
	{
		CDebugRenderer			&render = Level().debug_renderer();

		firedeps			fd;
		setup_firedeps		(fd);
		
		if(hud_adj_mode == 3 || bForce)
			render.draw_aabb(fd.vLastFP,0.005f,0.005f,0.005f,D3DCOLOR_XRGB(255,0,0));

		if(hud_adj_mode == 4)
			render.draw_aabb(fd.vLastFP2,0.005f,0.005f,0.005f,D3DCOLOR_XRGB(0,0,255));

		if(hud_adj_mode == 5)
			render.draw_aabb(fd.vLastSP,0.005f,0.005f,0.005f,D3DCOLOR_XRGB(0,255,0));
	}
#endif // DEBUG
}

void player_hud::tune(Ivector _values)
{
#ifndef MASTER_GOLD
	Ivector				values;
	tune_remap			(_values,values);

	bool is_16x9		= UI().is_widescreen();

	if(hud_adj_mode == 1)
	{
		Fvector			diff;
		diff.set		(0,0,0);
		
		float _curr_dr	= _delta_rot;

		u8 idx			= m_attached_items[hud_adj_item_idx]->m_parent_hud_item->GetCurrentHudOffsetIdx();
		if(idx)
			_curr_dr	/= 20.0f;

		Fvector& pos_	=(idx!=0)?m_attached_items[hud_adj_item_idx]->hands_offset_pos():m_attached_items[hud_adj_item_idx]->hands_attach_pos();
		Fvector& rot_	=(idx!=0)?m_attached_items[hud_adj_item_idx]->hands_offset_rot():m_attached_items[hud_adj_item_idx]->hands_attach_rot();

		if(hud_adj_offset == 0)
		{
			if(values.x)	diff.x = (values.x<0)?_delta_pos:-_delta_pos;
			if(values.y)	diff.y = (values.y>0)?_delta_pos:-_delta_pos;
			if(values.z)	diff.z = (values.z>0)?_delta_pos:-_delta_pos;

			pos_.add		(diff);
		}

		if (hud_adj_offset == 1)
		{
			if(values.x)	diff.x = (values.x>0)?_curr_dr:-_curr_dr;
			if(values.y)	diff.y = (values.y>0)?_curr_dr:-_curr_dr;
			if(values.z)	diff.z = (values.z>0)?_curr_dr:-_curr_dr;

			rot_.add		(diff);
		}
		if( (values.x)||(values.y)||(values.z) )
		{
			if(idx==0)
			{
				Msg("[%s]", m_attached_items[hud_adj_item_idx]->m_sect_name.c_str());
				Msg("hands_position%s				= %f,%f,%f",(is_16x9)?"_16x9":"", pos_.x, pos_.y, pos_.z);
				Msg("hands_orientation%s			= %f,%f,%f",(is_16x9)?"_16x9":"", rot_.x, rot_.y, rot_.z);
				Log("-----------");
			}
			else if(idx==1)
			{
				Msg("[%s]", m_attached_items[hud_adj_item_idx]->m_sect_name.c_str());
				Msg("aim_hud_offset_pos%s				= %f,%f,%f",(is_16x9)?"_16x9":"",  pos_.x, pos_.y, pos_.z);
				Msg("aim_hud_offset_rot%s				= %f,%f,%f",(is_16x9)?"_16x9":"",  rot_.x, rot_.y, rot_.z);
				Log("-----------");
			}
			else if(idx==2)
			{
				Msg("[%s]", m_attached_items[hud_adj_item_idx]->m_sect_name.c_str());
				Msg("gl_hud_offset_pos%s				= %f,%f,%f",(is_16x9)?"_16x9":"",  pos_.x, pos_.y, pos_.z);
				Msg("gl_hud_offset_rot%s				= %f,%f,%f",(is_16x9)?"_16x9":"",  rot_.x, rot_.y, rot_.z);
				Log("-----------");
			}
			else if(idx==3)
			{
				Msg("[%s]", m_attached_items[hud_adj_item_idx]->m_sect_name.c_str());
				Msg("alt_aim_hud_offset_pos%s				= %f,%f,%f",(is_16x9)?"_16x9":"",  pos_.x, pos_.y, pos_.z);
				Msg("alt_aim_hud_offset_rot%s				= %f,%f,%f",(is_16x9)?"_16x9":"",  rot_.x, rot_.y, rot_.z);
				Log("-----------");
			}
		}
	}
	else if(hud_adj_mode == 6)
	{
		if(hud_adj_offset == 0 && (values.z))
			_delta_pos	+= (values.z>0)?0.001f:-0.001f;
		
		if (hud_adj_offset == 1 && (values.z))
			 _delta_rot += (values.z>0)?0.1f:-0.1f;
	}
	else
	{
		attachable_hud_item* hi = m_attached_items[hud_adj_item_idx];
		if(!hi)	return;
		hi->tune(values);
	}
#endif // #ifndef MASTER_GOLD
}

LPCSTR _text()
{
	if (hud_adj_mode == 1)
	{
		if (hud_adj_offset == 0)
			return "adjusting HUD POSITION";
        else
			return "adjusting HUD ROTATION";
	}
	else if (hud_adj_mode == 2)
	{
		if (hud_adj_offset == 0)
			return "adjusting ITEM POSITION";
        else
			return "adjusting ITEM ROTATION";
	}
	else if (hud_adj_mode == 3)
		return "adjusting FIRE POINT";
	else if (hud_adj_mode == 4)
		return "adjusting FIRE 2 POINT";
	else if (hud_adj_mode == 5)
		return "adjusting SHELL POINT";
	else if (hud_adj_mode == 6)
	{
		if (hud_adj_offset == 0)
			return "adjusting pos STEP";
        else
			return "adjusting rot STEP";
	}
	else if (hud_adj_mode == 7)
		return "Choose as an adjust mode";
	else
		return NULL;
}

void hud_draw_adjust_mode()
{
    if (!hud_adj_active)
		return;

	if(_text())
	{
		CGameFont* F		= UI().Font().pFontDI;
		F->SetAligment		(CGameFont::alCenter);
		F->OutSetI			(0.f,-0.8f);
		F->SetColor			(0xffffffff);
		F->OutNext			(_text());
		F->OutNext			("for item [%d]", hud_adj_item_idx);
		F->OutNext			("delta values dP=%f dR=%f", _delta_pos, _delta_rot);
		F->OutNext			("[Z]-x axis [X]-y axis [C]-z axis");
	}
}

void hud_adjust_mode_keyb(int dik)
{
	//Эта функция мёртвая т.к. всё управление перенесено в ActorInput.cpp
}
