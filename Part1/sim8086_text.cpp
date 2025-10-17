#include "sim8086_text.h"
#include <iostream>
#include <iomanip>

template <typename T>
void TextSpace::PrintHex(int w, T t)
{
	std::cout << "0x" << std::hex << std::setfill('0') << std::setw(w) << t << std::dec;
}