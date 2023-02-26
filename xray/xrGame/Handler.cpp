///////////////////////////////////////////////////////////////
// Handler.h
// Handler - апгрейд оружия тактическая рукоятка
///////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "Handler.h"
//#include "PhysicsShell.h"

CHandler::CHandler(){}

CHandler::~CHandler(){}

BOOL CHandler::net_Spawn(CSE_Abstract* DC)
{
	return (inherited::net_Spawn(DC));
}

void CHandler::Load(LPCSTR section)
{
	inherited::Load(section);
}

void CHandler::net_Destroy()
{
	inherited::net_Destroy();
}

void CHandler::UpdateCL()
{
	inherited::UpdateCL();
}

void CHandler::OnH_A_Chield()
{
	inherited::OnH_A_Chield();
}

void CHandler::OnH_B_Independent(bool just_before_destroy)
{
	inherited::OnH_B_Independent(just_before_destroy);
}

void CHandler::renderable_Render()
{
	inherited::renderable_Render();
}