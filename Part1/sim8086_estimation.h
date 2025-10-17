#pragma once
#include <cstdint>

struct DecodedInstruction;

namespace Estimator
{
	void EstimateClocks(const DecodedInstruction& decodedInst, int32_t& estimatedClocks, int32_t& ea);
}

