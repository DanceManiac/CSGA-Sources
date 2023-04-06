// UI3dStatic.cpp: класс статического элемента, который рендерит 
// 3d объект в себя
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ui3dstatic.h"
#include "../gameobject.h"
#include "../HUDManager.h"
#include "../inventory_item.h"
#include "../../Include/xrRender/RenderVisual.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CUI3dStatic:: CUI3dStatic()
{
	m_x_angle = m_y_angle = m_z_angle = 0;
	Enable(false);
}

 CUI3dStatic::~ CUI3dStatic()
{

}

//расстояние от камеры до вещи, перед глазами
#define DIST  (VIEWPORT_NEAR + 0.1f)


void CUI3dStatic::FromScreenToItem(int x_screen, int y_screen, float& x_item, float& y_item)
{
		int x = x_screen;
		int y = y_screen;


        int halfwidth  = Device.dwWidth/2;
        int halfheight = Device.dwHeight/2;

	    float size_y = VIEWPORT_NEAR * tanf( deg2rad(1.f) * 0.5f);
        float size_x = size_y / (Device.fASPECT);

        float r_pt      = float(x-halfwidth) * size_x / (float) halfwidth;
        float u_pt      = float(halfheight-y) * size_y / (float) halfheight;

		x_item  = r_pt * DIST / VIEWPORT_NEAR;
		y_item = u_pt * DIST / VIEWPORT_NEAR;
}


//прорисовка
void  CUI3dStatic::Draw()
{
	if(m_pCurrentItem)
	{
		Frect rect;
		this->GetWndRect(rect);
		// Apply scale
		float rscale = m_pCurrentItem->cast_inventory_item()->RenderScale()*128;
		
		Fmatrix translate_matrix;
		Fmatrix scale_matrix;
		
		Fmatrix rx_m; 
		Fmatrix ry_m; 
		Fmatrix rz_m; 

		Fmatrix matrix;
		matrix.identity();


		//поместить объект в центр сферы
		translate_matrix.identity();
		translate_matrix.translate( - m_pCurrentItem->Visual()->getVisData().sphere.P.x,
			 					    - m_pCurrentItem->Visual()->getVisData().sphere.P.y,
								    - m_pCurrentItem->Visual()->getVisData().sphere.P.z);

		matrix.mulA_44(translate_matrix);


		rx_m.identity();
		rx_m.rotateX(m_x_angle);
		ry_m.identity();
		ry_m.rotateY(m_y_angle);
		rz_m.identity();
		rz_m.rotateZ(m_z_angle);


		matrix.mulA_44(rx_m);
		matrix.mulA_44(ry_m);
		matrix.mulA_44(rz_m);


		float x1, y1, x2, y2;

		FromScreenToItem(int(rect.left), int(rect.top), x1, y1);
		FromScreenToItem(int(rect.right), int(rect.bottom), x2, y2);

		float normal_size;
		normal_size =_abs(x2-x1)<_abs(y2-y1)?_abs(x2-x1):_abs(y2-y1);


		float radius = m_pCurrentItem->Visual()->getVisData().sphere.R;

		float scale = normal_size/(radius*2) * rscale;

		scale_matrix.identity();
		scale_matrix.scale( scale, scale,scale);

		matrix.mulA_44(scale_matrix);
        

		float right_item_offset, up_item_offset;

		
		///////////////////////////////	
		
		FromScreenToItem(int(rect.left) + iFloor(GetWidth()/2 * UI().GetScaleX()), int(rect.top) + iFloor(GetHeight()/2 * UI().GetScaleY()), right_item_offset, up_item_offset);

		translate_matrix.identity();
		translate_matrix.translate(right_item_offset,
								   up_item_offset,
								   DIST);

		matrix.mulA_44(translate_matrix);

		Fmatrix camera_matrix;
		camera_matrix.identity();
		camera_matrix = Device.mView;
		camera_matrix.invert();

		matrix.mulA_44(camera_matrix);

		
		::Render->set_Object(NULL); 
		::Render->set_Transform(&matrix);
		::Render->add_Visual(m_pCurrentItem->Visual());

		::Render->flush();
	}
}

void CUI3dStatic::SetGameObject(CGameObject* pItem)
{
	m_pCurrentItem = pItem;
}