#pragma once

#include <vector>
#include <string>
#include <bitset>
#include <memory>
#include <unordered_map>

struct DecodedInstruction;

enum class ExecutionType : uint8_t
{
	print,
	outFile,
	simulate,
	dump,
	showClocks,
	explainClocks
};

namespace Decoder
{
	void Disasm(DecodedInstruction& binaryInstruction);

	size_t FindWordIndex(const std::string& in, bool bWord);

	uint32_t GetEffectiveAddressIndex(int index, std::string& Address);

	bool CheckDispSpecialCon(const DecodedInstruction& decodedInst);

	// W = 0
	const std::vector<std::string> reg_rm_byte{ "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
	// W = 1
	const std::vector<std::string> reg_rm_word{ "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };

	// index <= 3 == baseReg + indexReg, index > 3 && index < 6 == indexReg, else == baseReg 
	const std::vector<std::string> effectiveAdress{ "bx + si", "bx + di", "bp + si", "bp + di", "si", "di", "bp", "bx" };

	static ExecutionType executionType{};
}

#define OPCODE_LIST \
	X(op_undefined) \
	X(op_mov) \
	X(op_add) \
	X(op_sub) \
	X(op_cmp) \
	X(op_je) \
	X(op_jl) \
	X(op_jle) \
	X(op_jb) \
	X(op_jbe) \
	X(op_jp) \
	X(op_jo) \
	X(op_js) \
	X(op_jne) \
	X(op_jnl) \
	X(op_jnle) \
	X(op_jnb) \
	X(op_jnbe) \
	X(op_jnp) \
	X(op_jno) \
	X(op_jns) \
	X(op_loop) \
	X(op_loopz) \
	X(op_loopnz) \
	X(op_jcxz) \
	X(op_test) \
	X(op_xor) \
	X(op_inc) \

enum class OpCode : uint8_t
{
#define X(name) name,
	OPCODE_LIST
#undef X
};

std::string OpcodeToString(OpCode opcode);

enum class OperandType
{
	ot_register,
	ot_memory,
	ot_immediate,
	ot_accumulator,
	ot_jumpTarget
};

struct DecodedInstruction
{
	DecodedInstruction();

	friend std::ostream& operator<<(std::ostream& out, const DecodedInstruction& decodedInst);

	std::string Dest{};
	std::string Source{};

	std::bitset<8> hi{};
	std::bitset<8> lo{};

	std::bitset<3> Reg{};
	std::bitset<3> RM{};
	std::bitset<2> MOD{};

	OpCode opCode = OpCode::op_undefined;
	OperandType DestOT = OperandType::ot_register;
	OperandType SourceOT = OperandType::ot_register;

	size_t memoryIndex = 0;
	size_t extraBits = 1;
	int8_t destTarget = 0;

	// w bit
	bool bWord = false;
	// d bit
	bool bRegIsDest = false;
	// s bit
	bool bSigned = false;
	bool bDisp = false;

	bool bPrintFlags = false;
};

struct VirtualChip
{
	inline uint16_t& operator[](size_t index)
	{
		return m_registers[index];
	}

	inline uint16_t& operator[](int index)
	{
		return m_registers[static_cast<size_t>(index)];
	}

	void AddUniqueMutatedRegister(size_t newReg);

	uint32_t* ip_register = nullptr;

	std::vector<uint8_t> m_memory{ std::vector<uint8_t>(1048576) };
	std::vector<uint16_t> m_registers{ std::vector<uint16_t>(8) };
	std::bitset<16> m_flags{};
	std::vector<char> m_flagSymbols{ 'C', {}, 'P', {}, 'A', {}, 'Z', 'S', 'T', 'I', 'D', 'O' };

	std::vector<size_t> m_mutatedRegisters{};

	int32_t totalClocks = 0;
};

inline VirtualChip virtualChip {};