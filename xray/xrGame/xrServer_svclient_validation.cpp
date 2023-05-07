#include "stdafx.h"
#include "xrServer_svclient_validation.h"
#include "Level.h"
#include "GameObject.h"

bool is_object_valid_on_svclient(u16 id_entity)
{
	CObject* tmp_obj		= Level().Objects.net_Find(id_entity);
	if (!tmp_obj)
		return false;
	
	CGameObject* tmp_gobj	= dynamic_cast<CGameObject*>(tmp_obj);
	if (!tmp_gobj)
		return false;

	if (tmp_obj->getDestroy())
		return false;

	if (tmp_gobj->object_removed())
		return false;
	
	return true;
};