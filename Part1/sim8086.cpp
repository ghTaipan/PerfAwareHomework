#include <string>
#include <cassert>
#include <iostream>

#include "sim8086.h"
#include "sim8086_decoder.h"
#include "sim8086_text.h"

static void SetFlags(DecodedInstruction& decodedInst, uint16_t NewVal, uint16_t OldDestVal, uint16_t SourceVal)
{
	decodedInst.bPrintFlags = true;

	// CF Flag
	if (decodedInst.opCode == OpCode::op_add ? OldDestVal > NewVal : OldDestVal < NewVal)
	{
		virtualChip.m_flags[0] = 1;
	}
	else
	{
		virtualChip.m_flags[0] = 0;
	}

	// PF Flag	
	if (std::bitset<8> (static_cast<uint8_t>(NewVal)).count() % 2 == 0)
	{
		virtualChip.m_flags[2] = 1;
	}
	else
	{
		virtualChip.m_flags[2] = 0;
	}

	// AF Flag
	if ((OldDestVal ^ SourceVal ^ NewVal) & 0x10)
	{
		virtualChip.m_flags[4] = 1;
	}
	else
	{
		virtualChip.m_flags[4] = 0;
	}

	// SF Flag
	if (NewVal & 0x8000)
	{
		virtualChip.m_flags[7] = 1;
	}
	else 
	{
		virtualChip.m_flags[7] = 0;
	}

	// ZF Flag
	if (NewVal == 0)
	{
		virtualChip.m_flags[6] = 1;
	}
	else
	{
		virtualChip.m_flags[6] = 0;
	}

	// OF Flag
	if (((decodedInst.opCode == OpCode::op_add ?
		~(OldDestVal ^ SourceVal) : (OldDestVal ^ SourceVal)) &
		(OldDestVal ^ NewVal)) & 0x8000)
	{
		virtualChip.m_flags[11] = 1;
	}
	else
	{
		virtualChip.m_flags[11] = 0;
	}
}

static size_t FillRegisterAddress(const std::string& regStr, uint16_t*& wordPtr, uint8_t*& bytePtr, bool bWord)
{
	const size_t index = Decoder::FindWordIndex(regStr, bWord);
	if (bWord)
	{
		wordPtr = &virtualChip[index];
	}
	else
	{
		if (index >= 0)
		{
			bytePtr = reinterpret_cast<uint8_t*>(&virtualChip[index]);
			if (index % 2 != 0)
			{
				bytePtr += 1;
			}
		}
	}

	return index;
}

static void FillMemoryAddress(uint16_t*& wordPtr, uint8_t*& bytePtr, size_t index, bool bWord)
{
	if (bWord)
	{
		wordPtr = reinterpret_cast<uint16_t*>(&virtualChip.m_memory[index]);
	}
	else
	{
		bytePtr = &virtualChip.m_memory[index];
	}
}

void Simulator::ExecuteInstruction(DecodedInstruction& decodedInst)
{
	uint16_t* destWord = nullptr;
	uint16_t* sourceWord = nullptr;
	uint8_t* destByte = nullptr;
	uint8_t* sourceByte = nullptr;

	uint16_t OldDestVal{};
	uint16_t immediateWord {};
	uint8_t immediateByte{};

	const bool bWord = decodedInst.bWord;

	virtualChip.ip_register += decodedInst.extraBits + 1;

	if (decodedInst.DestOT != OperandType::ot_jumpTarget)
	{
		if (decodedInst.DestOT == OperandType::ot_register || decodedInst.DestOT == OperandType::ot_accumulator)
		{
			virtualChip.AddUniqueMutatedRegister(FillRegisterAddress(decodedInst.Dest, destWord, destByte, bWord));
		}
		else if (decodedInst.DestOT == OperandType::ot_memory)
		{
			FillMemoryAddress(destWord, destByte, decodedInst.memoryIndex, bWord);
		}

		if (decodedInst.SourceOT == OperandType::ot_register || decodedInst.SourceOT == OperandType::ot_accumulator)
		{
			FillRegisterAddress(decodedInst.Source, sourceWord, sourceByte,bWord);
		}
		else if (decodedInst.SourceOT == OperandType::ot_immediate)
		{
			if (bWord)
			{
				immediateWord = static_cast<uint16_t>(std::stoi(decodedInst.Source));
				sourceWord = &immediateWord;
			}
			else
			{
				immediateByte = static_cast<uint8_t>(std::stoi(decodedInst.Source));
				sourceByte = &immediateByte;
			}
		}
		else if (decodedInst.SourceOT == OperandType::ot_memory)
		{
			FillMemoryAddress(sourceWord, sourceByte, decodedInst.memoryIndex, bWord);
		}

		if (destWord)
		{
			OldDestVal = *destWord;
		}
		else if (destByte)
		{
			OldDestVal += *destByte;

			if (bWord)
			{
				OldDestVal += *(destByte + 1);
			}
		}
	}

	switch (decodedInst.opCode)
	{
	case OpCode::op_mov:
		
		if (bWord)
		{
			assert(destWord && sourceWord);
			*destWord = *sourceWord;			
		}
		else
		{
			assert(destByte && sourceByte);
			*destByte = *sourceByte;
		}

		break;

	case OpCode::op_add:

		if (bWord)
		{
			assert(destWord && sourceWord);
			*destWord += *sourceWord;
			SetFlags(decodedInst, *destWord, OldDestVal, *sourceWord);
		}
		else
		{
			assert(destByte && sourceByte);
			*destByte += *sourceByte;
			SetFlags(decodedInst, static_cast<uint16_t>(*destByte), OldDestVal, static_cast<uint16_t>(*sourceByte));
		}

		break;

	case OpCode::op_sub:

		if (bWord)
		{
			assert(destWord && sourceWord);
			*destWord -= *sourceWord;
			SetFlags(decodedInst, *destWord, OldDestVal, *sourceWord);
		}
		else
		{
			assert(destByte && sourceByte);
			*destByte -= *sourceByte;
			SetFlags(decodedInst, static_cast<uint16_t>(*destByte), OldDestVal, static_cast<uint16_t>(*sourceByte));
		}

		break;

	case OpCode::op_cmp:

		if (bWord)
		{
			assert(destWord && sourceWord);
			SetFlags(decodedInst, *destWord - *sourceWord, OldDestVal, *sourceWord);
		}
		else
		{
			assert(destByte && sourceByte);
			*destByte -= *sourceByte;
			SetFlags(decodedInst, static_cast<uint16_t>(*destByte - *sourceByte), OldDestVal, static_cast<uint16_t>(*sourceByte));
		}

		break;
	
	case OpCode::op_je:
		if (virtualChip.m_flags[6])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jl:
		if (virtualChip.m_flags[7])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jle:
		if (virtualChip.m_flags[6] || virtualChip.m_flags[7])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jb:
		if (virtualChip.m_flags[1])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jbe:
		if (virtualChip.m_flags[1] || virtualChip.m_flags[6])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jp:
		if (virtualChip.m_flags[2])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jo:
		if (!virtualChip.m_flags[11])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case  OpCode::op_js:
		if (virtualChip.m_flags[7])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jne:
		if (!virtualChip.m_flags[6])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jnl:
		if (!virtualChip.m_flags[7])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jnle:
		if (!virtualChip.m_flags[6] && virtualChip.m_flags[7])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jnb:
		if (!virtualChip.m_flags[1])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jnbe:
		if (!virtualChip.m_flags[1] && !virtualChip.m_flags[6])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jnp:
		if (!virtualChip.m_flags[2])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jno:
		if (!virtualChip.m_flags[11])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_jns:
		if (virtualChip.m_flags[7])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;
	case OpCode::op_loop:
		if (virtualChip[1])
		{
			--virtualChip[1];
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;

	case OpCode::op_loopz:
		--virtualChip[1];
		if (virtualChip[1])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;

	case OpCode::op_loopnz:

		std::cout << "cx: ";
		TextSpace::PrintHex<uint16_t>(4, virtualChip[1]);
		std::cout << "->";
		--virtualChip[1];
		TextSpace::PrintHex<uint16_t>(4, virtualChip[1]);
		std::cout << " ";

		if (!virtualChip.m_flags[6] && virtualChip[1])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}

		break;

	case OpCode::op_jcxz:
		if (!virtualChip[1])
		{
			virtualChip.ip_register += decodedInst.destTarget;
		}
		break;

	default:
		break;
	}
}
