#pragma once

#include "Artefact.h"
#include "DetectList.h"
#include "hud_item_object.h"

class CUIArtefactDetectorBase;

class CAfList  :public CDetectList<CArtefact>
{
protected:
	virtual BOOL 	feel_touch_contact	(CObject* O);
public:
					CAfList		():m_af_rank(0){}
	int				m_af_rank;
};

class CCustomDetector :		public CHudItemObject
{
	typedef	CHudItemObject	inherited;
protected:
	CUIArtefactDetectorBase*			m_ui;
	bool			m_bFastAnimMode;

public:
					CCustomDetector		();
	virtual			~CCustomDetector	();

	enum EDetStates
	{
		eFireDet		= eLastBaseState+1,
		eKickKnf,
		eKickKnf2,
		eEmptyDet,
		eShowingParItm,
		eHideParItm,
		eThrowStartMis,
		eReadyMis,
		eThrowMis,
		eZoomStartDet,
		eZoomEndDet,
		eLookMisDet,
		eUnLightMisDet,
		eSwitchModeDet,
    };

	virtual BOOL 	net_Spawn			(CSE_Abstract* DC);
	virtual void 	Load				(LPCSTR section);

	virtual void 	OnH_A_Chield		();
	virtual void 	OnH_B_Independent	(bool just_before_destroy);

	virtual void 	shedule_Update		(u32 dt);
	virtual void 	UpdateCL			();


			bool 	IsWorking			();

	virtual void 	OnMoveToSlot		();
	virtual void 	OnMoveToRuck		(EItemPlace prev);

	virtual void	OnActiveItem		();
	virtual void	OnHiddenItem		();
	virtual void	OnStateSwitch		(u32 S);
	virtual void	SetHideDetStateInWeapon();
	virtual void	OnAnimationEnd		(u32 state);
	virtual	void	UpdateXForm			();

	void			ToggleDetector		(bool bFastMode);
	void			HideDetector		(bool bFastMode);
	void			ShowDetector		(bool bFastMode);
	float			m_fAfDetectRadius;
	virtual bool	CheckCompatibility	(CHudItem*);

	virtual u32		ef_detector_type	() const	{return 1;};
	void 	TurnDetectorInternal		(bool b);

	virtual bool TryPlayAnimIdle		();
	virtual void PlayAnimIdle			();

	bool			bZoomed;
	bool			m_bNeedActivation;

private:
	string64 guns_aim_det_anm;
protected:
	const	char*	GetAnimAimName				();
			bool	CheckCompatibilityInt		(CHudItem*);
	void 			UpdateNightVisionMode		(bool b_off);
	void			UpdateVisibility			();
	virtual void	UpfateWork					();
	virtual void 	UpdateAf					()				{};
	virtual void 	CreateUI					()				{};

	bool			m_bWorking;
	float			m_fAfVisRadius;

	CAfList			m_artefacts;
};
