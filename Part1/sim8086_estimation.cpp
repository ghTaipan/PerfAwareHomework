#include "sim8086_estimation.h"
#include "sim8086_decoder.h"

static int8_t EA(const DecodedInstruction& decodedInst)
{
	if (decodedInst.DestOT != OperandType::ot_memory && decodedInst.SourceOT != OperandType::ot_memory)
	{
		return 0;
	}

	const uint8_t RM = static_cast<uint8_t>(decodedInst.RM.to_ulong());

	if (Decoder::CheckDispSpecialCon(decodedInst))
	{
		return 6; // Dips only
	}

	if (decodedInst.MOD == 0b11 || decodedInst.DestOT == OperandType::ot_accumulator || decodedInst.SourceOT == OperandType::ot_accumulator)
	{
		return 0;
	}

	if (decodedInst.bDisp && decodedInst.memoryIndex > 0)
	{
		if (RM <= 3)
		{
			if (RM == 3 || RM == 0) // BP + DI or BX + SI
			{
				return 11;
			}
			else // BX + DI or BP + SI
			{
				return 12;
			}
		}
		else // base or index
		{
			return 9;
		}
	}
	else
	{
		if (RM == 3 || RM == 0) // BP + DI or BX + SI
		{
			return 7;
		}
		else if (RM == 1 || RM == 2) // BX + DI or BP + SI
		{
			return 8;
		}
		
		return 5; // base or index
	}
}

static void Estimator::EstimateClocks(const DecodedInstruction& decodedInst, int32_t& estimatedClocks, int32_t& ea)
{
	ea = EA(decodedInst);

	switch (decodedInst.opCode)
	{
	case OpCode::op_mov:
		switch (decodedInst.DestOT)
		{
		case OperandType::ot_register:
			switch (decodedInst.SourceOT)
			{
			case OperandType::ot_register:
				estimatedClocks = 2;
				break;
			case OperandType::ot_memory:
				estimatedClocks = 8;
				
				break;
			default: // immediate
				estimatedClocks = 4;
				break;
			}

			break;

		case OperandType::ot_memory:
			switch (decodedInst.SourceOT)
			{
			case OperandType::ot_accumulator:
				estimatedClocks = 10;
				break;
			case OperandType::ot_register:
				estimatedClocks = 9;
				break;
			case OperandType::ot_immediate:
				estimatedClocks = 10;
				break;
			default: // seg_reg
				estimatedClocks = 9;
				break;
			}

			break;

		case OperandType::ot_accumulator: // SourceOT == memory
			estimatedClocks = 10;
			break;

		default: // seg_reg
			if (decodedInst.SourceOT == OperandType::ot_register)
			{
				estimatedClocks = 2;
			}
			else
			{
				estimatedClocks = 8;
			}

			break;
		}
		
		break;

	case OpCode::op_add:
	case OpCode::op_sub:
		switch (decodedInst.DestOT)
		{
		case OperandType::ot_register:
			switch (decodedInst.SourceOT)
			{
			case OperandType::ot_register:
				estimatedClocks = 3;
				break;
			case OperandType::ot_memory:
				estimatedClocks = 9;
				break;
			default: // immediate
				estimatedClocks = 4;
				break;
			}

			break;

		case OperandType::ot_memory:
			switch (decodedInst.SourceOT)
			{
			case OperandType::ot_register:
				estimatedClocks = 16;
				break;
			default: // immediate
				estimatedClocks = 17;
				break;
			}

			break;

		default: // acc
			estimatedClocks = 4;
			break;
		}

		break;

	case OpCode::op_cmp:
		switch (decodedInst.DestOT)
		{
		case OperandType::ot_register:
			switch (decodedInst.SourceOT)
			{
			case OperandType::ot_register:
				estimatedClocks = 3;
				break;
			case OperandType::ot_memory:
				estimatedClocks = 9;
				break;
			default: // immediate
				estimatedClocks = 4;
				break;
			}

			break;

		case OperandType::ot_memory:
			switch (decodedInst.SourceOT)
			{
			case OperandType::ot_register:
				estimatedClocks = 9;
				break;
			default: // immediate
				estimatedClocks = 10;
				break;
			}

			break;

		default: // acc
			estimatedClocks = 4;
			break;
		}

		break;

	default:
		return;
	}
}
