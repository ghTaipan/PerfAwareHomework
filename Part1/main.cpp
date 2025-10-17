#include <cassert>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include "sim8086.h"
#include "sim8086_decoder.h"

#include "sim8086_estimation.h"
#include "sim8086_estimation.cpp"

#include "sim8086_text.h"
#include "sim8086_text.cpp"


int main(int argc, char* argv[])
{
    assert(argc >= 2 && "A filename is needed to specified!");


    std::ofstream outf{};

    // set execution type and read binary file.
    if (argc > 2)
    {
        
        const std::string arg = std::string(argv[1]);
        if (arg == "-exec")
        {
            Decoder::executionType = ExecutionType::simulate;            
        }
        else if (arg == "-dump")
        {
            Decoder::executionType = ExecutionType::dump;
        }
        else if (arg == "-showclocks")
        {
            Decoder::executionType = ExecutionType::showClocks;
        }
        else if (arg == "-explainclocks")
        {
            Decoder::executionType = ExecutionType::explainClocks;
        }
        else
        {
            Decoder::executionType = ExecutionType::outFile;
        }
    }
    else
    {
        Decoder::executionType = ExecutionType::print;
    }

    std::ifstream inf{ argv[argc - 1] };
    if (!inf)
    {
        std::cout << argv[argc - 1] << " could not be opened for reading!";
        return -1;
    }

    std::vector<uint32_t> buffer{ std::istreambuf_iterator<char>(inf), {} };
    uint32_t bufferSize = static_cast<uint32_t>(buffer.size());
    uint32_t* binaryInstructionStream = buffer.data();

    switch (Decoder::executionType)
    {
    case ExecutionType::print:
        std::cout << argv[argc - 1] << " disassembly:\nbits 16\n";
        break;

    case ExecutionType::outFile:
        outf.open(argv[argc - 1]);
        outf << "bits 16\n";
        break;

    default:
        if (Decoder::executionType == ExecutionType::dump)
        {
            int32_t i = 0;
            std::string filePath = std::format("sim8086_memory_{}.data", i);
            while (std::filesystem::is_regular_file(filePath))
            {
                ++i;
                filePath = "sim8086_memory_" + std::to_string(i) + ".data";
            }

            outf.open(filePath, std::ios::binary);
        }

        std::cout << argv[argc - 1] << " execution \n";
        break;
    }

    uint32_t* startPtr = &binaryInstructionStream[0];
    virtualChip.ip_register = startPtr;

    while ((virtualChip.ip_register - startPtr) < bufferSize)
    {;
        uint32_t* oldIp = virtualChip.ip_register;

        DecodedInstruction decodedInst;
        Decoder::Disasm(decodedInst);

        // Output according to execution type.
        switch (Decoder::executionType)
        {
        case ExecutionType::print:
            std::cout << decodedInst << '\n';
            break;

        case ExecutionType::outFile:
            outf << decodedInst << '\n';
            break;

        default:

            std::bitset<16> oldFlags{ virtualChip.m_flags };
            std::cout << decodedInst << " ; ";

            if (Decoder::executionType >= ExecutionType::showClocks)
            {
                int32_t estimatedClocks = 0;
                int32_t ea = 0;
                Estimator::EstimateClocks(decodedInst, estimatedClocks, ea);

                const int32_t sumClocks = estimatedClocks + ea;
                virtualChip.totalClocks += sumClocks;

                std::cout << " Clocks: +" << std::to_string(sumClocks) << " = " << std::to_string(virtualChip.totalClocks) <<
                    (ea > 0 && Decoder::executionType == ExecutionType::explainClocks ?
                        std::format(" ({} + {}ea)", estimatedClocks, ea) : "") << " | ";
            }

            if (decodedInst.DestOT != OperandType::ot_register || (decodedInst.opCode >= OpCode::op_je && decodedInst.opCode <= OpCode::op_jcxz))
            {
                Simulator::ExecuteInstruction(decodedInst);
                std::cout << "ip:";
            }
            else
            {
                const size_t RegisterIndex = Decoder::FindWordIndex(decodedInst.Dest, decodedInst.bWord);

                std::cout << Decoder::reg_rm_word[RegisterIndex];
                std::cout << ':';
                TextSpace::PrintHex(4, virtualChip[RegisterIndex]);
                std::cout << "->";

                Simulator::ExecuteInstruction(decodedInst);
                TextSpace::PrintHex(4, virtualChip[RegisterIndex]);
                std::cout << " ip:";
            }

            // print ip register's old and new distance to starting pointer.

            TextSpace::PrintHex(2, std::distance(startPtr, oldIp));
            std::cout << "->";
            TextSpace::PrintHex(2, std::distance(startPtr, virtualChip.ip_register));

            if (decodedInst.bPrintFlags)
            {
                std::cout << " flags:";

                std::string oldFlagsStr{};
                std::string newFlagsStr{};


                for (size_t i{ 0 }; i < 16; ++i)
                {
                    if (oldFlags[i])
                    {
                        oldFlagsStr += virtualChip.m_flagSymbols[i];
                    }

                    if (virtualChip.m_flags[i])
                    {
                        newFlagsStr += virtualChip.m_flagSymbols[i];
                    }
                }

                std::cout << oldFlagsStr << "->" << newFlagsStr;
            }

            std::cout << '\n';

            break;             
        }
    }

    // final version of registers and flags.
    if (Decoder::executionType == ExecutionType::dump)
    {
        outf.write(reinterpret_cast<const char*>(virtualChip.m_memory.data()), virtualChip.m_memory.size());
    }
    if (Decoder::executionType >= ExecutionType::simulate)
    {
        std::cout << "\nFinal registers:\n";

        for (size_t i : virtualChip.m_mutatedRegisters)
        {
            const int32_t Val = static_cast<int32_t>(virtualChip[i]);

            if (Val == 0)
            {
                continue;
            }

            std::cout << "      " << Decoder::reg_rm_word[i] << ": ";
            TextSpace::PrintHex(4, virtualChip[i]);
            std::cout << " (" << static_cast<int32_t>(virtualChip[i]) << ")\n";
        }

        int32_t distance = static_cast<int32_t>(std::distance(startPtr, virtualChip.ip_register));

        std::cout << "      ip:";
        TextSpace::PrintHex(4, distance);
        std::cout << " (" << distance << ")";

        if (virtualChip.m_flags.count() > 0)
        {
            std::cout << "\n  flags :";
            for (size_t i{ 0 }; i < 16; ++i)
            {
                if (virtualChip.m_flags[i])
                {
                    std::cout << virtualChip.m_flagSymbols[i];
                }
            }
        }

        std::cout << '\n';
    }

    return 0;
}