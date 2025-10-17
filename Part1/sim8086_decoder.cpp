#include "sim8086_decoder.h"
#include <cassert>
#include <iostream>

void VirtualChip::AddUniqueMutatedRegister(size_t newRegister)
{
    auto cond = [newRegister](const size_t& t) {return t == newRegister;};

    auto it = std::find_if(m_mutatedRegisters.begin(), m_mutatedRegisters.end(), cond);

    if (it == m_mutatedRegisters.end())
    {
        m_mutatedRegisters.push_back(newRegister);
    }
}

uint32_t Decoder::GetEffectiveAddressIndex(int index, std::string& address)
{
    uint32_t addressIndex = 0;
    address += Decoder::effectiveAdress[index];

    addressIndex += virtualChip[Decoder::FindWordIndex(address.substr(1, 2), true)];

    if (index <= 3)
    {
        addressIndex += virtualChip[Decoder::FindWordIndex(address.substr(6, 7), true)];
    }

    return addressIndex;
}

size_t Decoder::FindWordIndex(const std::string& in, bool bWord)
{
    const std::string searchStr = bWord ? in : in.substr(0, 1) + "x";

    for (size_t i{ 0 }; i < 8; ++i)
    {
        if (Decoder::reg_rm_word[i] == searchStr)
        {
            return i;
        }
    }

    return -1;
}

DecodedInstruction::DecodedInstruction()
{
    assert(virtualChip.ip_register);

    hi = *virtualChip.ip_register & 0xFF;
    lo = *(virtualChip.ip_register + 1) & 0xFF;
}

std::string OpcodeToString(OpCode opcode)
{
    std::string result;
    switch (opcode) {
#define X(name) case OpCode::name: \
        result = #name; \
        result = result.substr(3); \
        break;
            OPCODE_LIST
#undef X
    default: result = "Unknown";
            break;
    }

    return result;
}

std::ostream& operator<<(std::ostream& out, const DecodedInstruction& decodedInst)
{
    out << OpcodeToString(decodedInst.opCode) << " " + decodedInst.Dest;

    if (decodedInst.Source.length() > 0)
    {
        out << ", " + decodedInst.Source;
    }

    return out;
}

static std::string getRegisterName(const std::bitset<3>& bits, bool bWord)
{
    const size_t index = static_cast<size_t>(bits.to_ulong() & 0b111);
    return bWord ? Decoder::reg_rm_word[index] : Decoder::reg_rm_byte[index];
}

static int16_t GetTwoByteImmediateFromInst(uint32_t* binaryInst)
{
    return static_cast<int16_t>( (((*(binaryInst + 1) << 8) | (*(binaryInst) & 0xFF)) & 0xFFFF) );
}

static std::string GetTwoByteImmediateStrFromInst(uint32_t* binaryInst)
{
    return std::to_string(GetTwoByteImmediateFromInst(binaryInst));
}

static void SetRegistersFromMODOneOne(DecodedInstruction& decodedInst, bool bImmediateSource = false)
{
    if (decodedInst.MOD == 0b11)
    {
        if (decodedInst.bRegIsDest)
        {
            if (!bImmediateSource)
            {
                decodedInst.Dest += getRegisterName(decodedInst.Reg, decodedInst.bWord);
                decodedInst.Source += getRegisterName(decodedInst.RM, decodedInst.bWord);
                decodedInst.SourceOT = OperandType::ot_memory;
            }
            else
            {
                decodedInst.Dest += getRegisterName(decodedInst.RM, decodedInst.bWord);
            }
        }
        else
        {
            decodedInst.Dest += getRegisterName(decodedInst.RM, decodedInst.bWord);
            if (!bImmediateSource)
            {
                decodedInst.Source += getRegisterName(decodedInst.Reg, decodedInst.bWord);
            }
        }
    }
}

static std::string GetEffectiveAddressFromMOD(DecodedInstruction& decodedInst)
{
    assert(decodedInst.MOD != 0b11);

    const int index = static_cast<int>(decodedInst.RM.to_ulong());


    std::string returnVal {'['};
    Decoder::GetEffectiveAddressIndex(index, returnVal);
    std::string dispMem{};

    if (decodedInst.MOD.to_ulong() == 0b00)
    {
        if (decodedInst.RM.to_ulong() == 0b110)
        {
            decodedInst.bDisp = true;
            decodedInst.memoryIndex = GetTwoByteImmediateFromInst(virtualChip.ip_register + 2);
            return "[" + std::to_string(decodedInst.memoryIndex) + ']';
        }
    }
    else
    {
        decodedInst.bDisp = true;

        if (decodedInst.MOD.to_ulong() == 0b01)
        {
            decodedInst.memoryIndex += static_cast<int8_t>((*(virtualChip.ip_register + 2) & 0xFF));
        }
        else
        {
            decodedInst.memoryIndex += GetTwoByteImmediateFromInst(virtualChip.ip_register + 2);
        }

        dispMem = std::to_string(decodedInst.memoryIndex);
        if (dispMem != "0")
        {
            if (dispMem[0] == '-')
            {
                dispMem.erase(dispMem.begin());
                returnVal += " - " + dispMem;
            }
            else
            {
                returnVal += " + " + dispMem;
            }
        }
    }

    returnVal += "]";

    return returnVal;
}

// MOD == 00 && RM == 110
bool Decoder::CheckDispSpecialCon(const DecodedInstruction& decodedInst)
{
    return decodedInst.MOD == 0b00 && decodedInst.RM == 0b110;
}

// Register/Memory to/from Register
static void RegMemToFromReg(DecodedInstruction& decodedInst)
{
    decodedInst.bWord = decodedInst.hi[0];
    decodedInst.bRegIsDest = decodedInst.hi[1];
    decodedInst.Reg = (decodedInst.lo.to_ulong() >> 3) & 0b111;
    decodedInst.RM = decodedInst.lo.to_ulong() & 0b111;
    decodedInst.MOD = decodedInst.lo.to_ulong() >> 6;

    if (decodedInst.MOD == std::bitset<2>(0b11))
    {
        SetRegistersFromMODOneOne(decodedInst);
    }
    else
    {
        decodedInst.extraBits += decodedInst.MOD.to_ulong();

        if (Decoder::CheckDispSpecialCon(decodedInst))
        {
            decodedInst.extraBits += 2;
        }

        // Reg is dest?
        if (decodedInst.bRegIsDest)
        {
            decodedInst.Dest = getRegisterName(decodedInst.Reg, decodedInst.bWord);
            decodedInst.Source = GetEffectiveAddressFromMOD(decodedInst);
            decodedInst.SourceOT = OperandType::ot_memory;
        }
        else
        {
            decodedInst.Dest = GetEffectiveAddressFromMOD(decodedInst);
            decodedInst.DestOT = OperandType::ot_memory;
            decodedInst.Source = getRegisterName(decodedInst.Reg, decodedInst.bWord);
        }
    }
}

static void ImmToRegMem(DecodedInstruction& decodedInst, bool bSWForData = false)
{
    decodedInst.bWord = decodedInst.hi[0];
    decodedInst.bRegIsDest = decodedInst.hi[1];
    decodedInst.MOD = decodedInst.lo.to_ulong() >> 6;
    decodedInst.RM = decodedInst.lo.to_ulong() & 0b111;
    decodedInst.bSigned = bSWForData ? decodedInst.hi[1] : false;
    decodedInst.SourceOT = OperandType::ot_immediate;

    bool bImmediatePrefix = decodedInst.MOD != 0b11;

    if ((decodedInst.MOD.to_ulong() == 0b10 || Decoder::CheckDispSpecialCon(decodedInst)))
    {
        decodedInst.extraBits += 2;
    }
    else if (decodedInst.MOD.to_ulong() == 0b01)
    {
        decodedInst.extraBits += 1;
    }

    decodedInst.bDisp = true;

    std::string immediateStr{};
    if (decodedInst.bWord && (!bSWForData || !decodedInst.bSigned))
    {
        decodedInst.extraBits += 2;

        if (bImmediatePrefix)
        {
            decodedInst.Dest = "word ";
        }

        decodedInst.Source += GetTwoByteImmediateStrFromInst(virtualChip.ip_register + decodedInst.extraBits - 1);
    }
    else
    {
        decodedInst.extraBits += 1;

        if (bImmediatePrefix)
        {
            decodedInst.Dest = "byte ";
        }

        decodedInst.Source += std::to_string(static_cast<int8_t>(
            *(virtualChip.ip_register + decodedInst.extraBits) & 0xFF ));
    }

    if (decodedInst.MOD == 0b11)
    {
        SetRegistersFromMODOneOne(decodedInst, true);
    }
    else
    {
        decodedInst.Dest += GetEffectiveAddressFromMOD(decodedInst);
        decodedInst.DestOT = OperandType::ot_memory;
    }

}

static void ImmToAcc(DecodedInstruction& decodedInst, bool bCanBeWord = true)
{
    decodedInst.bWord = bCanBeWord && decodedInst.hi[0];

    if (decodedInst.bWord)
    {
        ++decodedInst.extraBits;
        decodedInst.Dest = "ax";
        decodedInst.Source = GetTwoByteImmediateStrFromInst(virtualChip.ip_register + decodedInst.extraBits - 1);
    }
    else
    {
        decodedInst.Dest = "al";
        decodedInst.Source = std::to_string(static_cast<int8_t>(*(virtualChip.ip_register + decodedInst.extraBits) & 0xFF));
    }

    decodedInst.DestOT = OperandType::ot_accumulator;
    decodedInst.SourceOT = OperandType::ot_immediate;
}

void Decoder::Disasm(DecodedInstruction& decodedInst)
{
    switch (decodedInst.hi.to_ulong())
    {
    case 0b01110100:
        decodedInst.opCode = OpCode::op_je;
        break;
    case 0b01111100:
        decodedInst.opCode = OpCode::op_jl;
        break;
    case 0b01111110:
        decodedInst.opCode = OpCode::op_jle;
        break;
    case 0b01110010:
        decodedInst.opCode = OpCode::op_jb;
        break;
    case 0b01110110:
        decodedInst.opCode = OpCode::op_jbe;
        break;
    case 0b01111010:
        decodedInst.opCode = OpCode::op_jp;
        break;
    case 0b01110000:
        decodedInst.opCode = OpCode::op_jo;
        break;
    case 0b01111000:
        decodedInst.opCode = OpCode::op_js;
        break;
    case 0b01110101:
        decodedInst.opCode = OpCode::op_jne;
        break;
    case 0b01111101:
        decodedInst.opCode = OpCode::op_jnl;
        break;
    case 0b01111111:
        decodedInst.opCode = OpCode::op_jnle;
        break;
    case 0b01110011:
        decodedInst.opCode = OpCode::op_jnb;
        break;
    case 0b01110111:
        decodedInst.opCode = OpCode::op_jnbe;
        break;
    case 0b01111011:
        decodedInst.opCode = OpCode::op_jnp;
        break;
    case 0b01110001:
        decodedInst.opCode = OpCode::op_jno;
        break;
    case 0b01111001:
        decodedInst.opCode = OpCode::op_jns;
        break;
    case 0b11100010:
        decodedInst.opCode = OpCode::op_loop;
        break;
    case 0b11100001:
        decodedInst.opCode = OpCode::op_loopz;
        break;
    case 0b11100000:
        decodedInst.opCode = OpCode::op_loopnz;
        break;
    case 0b11100011:
        decodedInst.opCode = OpCode::op_jcxz;
        break;
    default:
        break;
    }

    if (decodedInst.opCode != OpCode::op_undefined)
    {
        decodedInst.destTarget = static_cast<int8_t>(*(virtualChip.ip_register + 1) & 0xFF);
        decodedInst.DestOT = OperandType::ot_jumpTarget;

        decodedInst.Dest = "$";
        if (decodedInst.destTarget >= 0)
        {
            decodedInst.Dest += "+";
        }
        decodedInst.Dest += std::to_string(decodedInst.destTarget + 2);

        return;
    }

    switch (decodedInst.hi.to_ulong() >> 1)
    {
    // mov immediate to reg/mem
    case 0b1100011:
        decodedInst.opCode = OpCode::op_mov;
        ImmToRegMem(decodedInst);
        return;

    // mov mem to acc
    case 0b1010000:
        ++decodedInst.extraBits;
        decodedInst.bWord = decodedInst.hi[0];

        decodedInst.opCode = OpCode::op_mov;
        decodedInst.Dest = (decodedInst.bWord ? "ax" : "al");
        decodedInst.DestOT = OperandType::ot_accumulator;

        decodedInst.Source = '[' + GetTwoByteImmediateStrFromInst(virtualChip.ip_register + 1) + ']';
        decodedInst.SourceOT = OperandType::ot_memory;
        return;

     // mov acc to mem
    case 0b1010001:
        ++decodedInst.extraBits;

        decodedInst.opCode = OpCode::op_mov;

        decodedInst.Source = (decodedInst.hi[0] ? "ax" : "al");
        decodedInst.SourceOT = OperandType::ot_accumulator;

        decodedInst.Dest = '[' + GetTwoByteImmediateStrFromInst(virtualChip.ip_register + 1) + ']';
        decodedInst.DestOT = OperandType::ot_memory;
        return;

    // add immediate to acc
    case 0b0000010:
        decodedInst.opCode = OpCode::op_add;
        ImmToAcc(decodedInst);
        return;

    // sub immediate to acc
    case 0b0010110:
        decodedInst.opCode = OpCode::op_sub;
        ImmToAcc(decodedInst);
        return;

    // cmp immediate to acc
    case 0b0011110:
        decodedInst.opCode = OpCode::op_cmp;
        ImmToAcc(decodedInst);
        return;

    // test immediate and reg/mem
    case 0b1111011:
        decodedInst.opCode = OpCode::op_test;
        ImmToRegMem(decodedInst);
        return;
    // test immediate and acc
    case 0b1010100:
        decodedInst.opCode = OpCode::op_test;
        ImmToAcc(decodedInst, false);
        return;
    default:
        break;
    }

    switch (decodedInst.hi.to_ulong() >> 2)
    {
    // mov reg/mem to/from reg
    case 0b100010:
        decodedInst.opCode = OpCode::op_mov;
        RegMemToFromReg(decodedInst);
        return;

    // add reg/mem with reg to either
    case 0b000000:
        decodedInst.opCode = OpCode::op_add;
        RegMemToFromReg(decodedInst);
        return;

    // sub reg/mem with reg to either
    case 0b001010:
        decodedInst.opCode = OpCode::op_sub;
        RegMemToFromReg(decodedInst);
        return;
    
    // cmp reg/mem with reg to either
    case 0b001110:  
        decodedInst.opCode = OpCode::op_cmp;
        RegMemToFromReg(decodedInst);
        return;

    case 0b100000:
        switch ((decodedInst.lo.to_ulong() >> 3) & 0b111)
        {
        // add immediate to reg/mem
        case 0b000:
            decodedInst.opCode = OpCode::op_add;
            ImmToRegMem(decodedInst, true);
            return;

        // sub immediate to reg/mem
        case 0b101:
            decodedInst.opCode = OpCode::op_sub;
            ImmToRegMem(decodedInst, true);
            return;

        // cmp immediate to reg/mem
        case 0b111:
            decodedInst.opCode = OpCode::op_cmp;
            ImmToRegMem(decodedInst, true);
            return;

        default:
            std::cout << "Undefined register!\n";
            return;
        }
        
        return;

    // test reg/mem with reg to either
    case 0b000100:
        decodedInst.opCode = OpCode::op_test;
        RegMemToFromReg(decodedInst);
        return;

    default:
        break;
    }

    // mov immediate to reg
    if (decodedInst.hi >> 4 == 0b1011)
    {
        decodedInst.bWord = decodedInst.hi[3];
        decodedInst.Reg = decodedInst.hi.to_ulong() & 0b111;

        decodedInst.opCode = OpCode::op_mov;
        decodedInst.Dest = getRegisterName(decodedInst.Reg, decodedInst.bWord);

        decodedInst.SourceOT = OperandType::ot_immediate;

        if (decodedInst.bWord)
        {
            ++decodedInst.extraBits;
            decodedInst.Source = GetTwoByteImmediateStrFromInst(virtualChip.ip_register + 1);
        }
        else
        {
            decodedInst.Source = std::to_string(static_cast<int16_t>(
                *(virtualChip.ip_register + 1) & 0xFFFF));
        }

        return;
    }

    std::cout << "Undefined register!\n";
}