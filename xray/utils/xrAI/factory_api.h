#pragma once

#include "../xrSE_Factory/xrSE_Factory_import_export.h"

IC	CSE_Abstract *F_entity_Create(LPCSTR section)
{
	ISE_Abstract	*i = create_entity(section);
	CSE_Abstract	*j = dynamic_cast<CSE_Abstract*>(i);
	return			(j);
}

IC	void F_entity_Destroy(CSE_Abstract *&i)
{
	ISE_Abstract	*j = i;
	destroy_entity	(j);
	i				= 0;
}
