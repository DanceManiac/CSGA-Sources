#include "StdAfx.h"
#include "GameConstants.h"
#include "GamePersistent.h"

float costPenaltyPercentage_1_ = 0.f;
float costPenaltyPercentage_2_ = 0.f;
float costPenaltyPercentage_3_ = 0.f;
float costPenaltyPercentage_4_ = 0.f;

float eatableContainerWeightPerc_ = 0.f;

namespace GameConstants
{
	void LoadConstants()
	{

		costPenaltyPercentage_1_	= READ_IF_EXISTS(pSettings,r_float,"inventory", "portions_cost_penalty_1",0);
		costPenaltyPercentage_2_	= READ_IF_EXISTS(pSettings, r_float, "inventory", "portions_cost_penalty_2", 0);
		costPenaltyPercentage_3_	= READ_IF_EXISTS(pSettings, r_float, "inventory", "portions_cost_penalty_3", 0);
		costPenaltyPercentage_4_	= READ_IF_EXISTS(pSettings, r_float, "inventory", "portions_cost_penalty_4", 0);

		eatableContainerWeightPerc_	= pSettings->r_float("inventory", "eatable_item_container_weight_percentage");

		Msg("* GameConstants are Loaded");
	}

	float GetCostPenaltyForMissingPortions(u8 num_of_missing_portions)
	{
		if (num_of_missing_portions == 0)
			return 0.f;

		switch (num_of_missing_portions)
		{
		case 1:
			return costPenaltyPercentage_1_;
		case 2:
			return costPenaltyPercentage_2_;
		case 3:
			return costPenaltyPercentage_3_;
		case 4:
			return costPenaltyPercentage_4_;
		default:
			return costPenaltyPercentage_4_;
		}
	}

	float GetEIContainerWeightPerc()
	{
		return eatableContainerWeightPerc_;
	}
}