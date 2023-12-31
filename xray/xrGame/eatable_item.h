#pragma once

#include "inventory_item.h"

class CPhysicItem;
class CEntityAlive;

class CEatableItem : public CInventoryItem {
private:
	typedef CInventoryItem	inherited;

private:
	CPhysicItem		*m_physic_item;

public:
							CEatableItem				();
	virtual					~CEatableItem				();
	virtual	DLL_Pure*		_construct					();
	virtual CEatableItem	*cast_eatable_item			()	{return this;}

	virtual void			Load						(LPCSTR section);
			bool			Empty						()	const	{ return m_iPortionsNum==0; };
	virtual bool			Useful						() const;

	virtual BOOL			net_Spawn					(CSE_Abstract* DC);	
	virtual void			save						(NET_Packet &output_packet);
	virtual void			load						(IReader &input_packet);

	virtual void			OnH_B_Independent			(bool just_before_destroy);
	virtual void			OnH_A_Independent			();
	virtual	void			UseBy						(CEntityAlive* npc);
	virtual	bool			Empty						()						{return GetPortionsNum()==0;};
			int				GetPortionsNum				()	const				{return m_iPortionsNum;}
	IC	u32					GetBasePortionsNum			()	const				{return m_iStartPortionsNum; }

	virtual float			Weight						() const;
	virtual	u32				Cost						() const;
protected:	
	//влияние при поедании вещи на параметры игрока
	float					m_fHealthInfluence;
	float					m_fPowerInfluence;
	float					m_fSatietyInfluence;
	float					m_fRadiationInfluence;
	float					m_fMaxPowerUpInfluence;
	//заживление ран на кол-во процентов
	float					m_fWoundsHealPerc;

	//количество порций еды, 
	//-1 - порция одна и больше не бывает (чтоб не выводить надпись в меню)
	int						m_iPortionsNum;
	int						m_iStartPortionsNum;
};

