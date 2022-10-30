#pragma once

namespace GameConstants
{
	void				LoadConstants();

	// Gets the aditional cost penalty persentage for X amount of missing eatable item portions
	float				GetCostPenaltyForMissingPortions(u8 num_of_missing_portions);

	// Gets the "empty eatable item" weight (ex. weight of bottle without water)
	float				GetEIContainerWeightPerc();
};