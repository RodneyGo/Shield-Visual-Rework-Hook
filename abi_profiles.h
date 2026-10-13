// Hashes only; generated from the audited 9.00/611726 executable.
#pragma once
struct ABISection { unsigned rva, size, virtualSize, flags; const char* hash; };
inline constexpr ABISection ABISections[] = {
    {4096, 29095936, 29095692, 1610612768u, "e37da1d87164c4c58f12208eaf989312901422d4d3a04106f6861a8a1bad7165"}, // .text
    {29102080, 20423680, 20423296, 1073741888u, "772c3157a3e345fa204c9b870a4cb57f8b66486c2b44da62ec5476ffb4f4ec0e"}, // .rdata
    {49528832, 3255808, 64921920, 3221225536u, "a8163abdd0ebc20f7248cf02bf27032e74f1777069bb9ac3db0034c7b2fb0fdd"}, // .data
    {114454528, 991232, 990852, 1073741888u, "fd1c184aebe6f85bd155fe48561ad180276db06fa3b79a0a61b11c53925de23b"}, // .pdata
    {115445760, 78848, 78608, 1073741888u, "b9693d98596b01e1054f2608ff33d67cb93189afc4951fd108735e63b96d5f4b"}, // _RDATA
    {115654656, 1428992, 1428768, 1107296320u, "0947cc85a5295d176618120bcd9741fbf73471cd21019121884d71c9f63e1398"}, // .reloc
};
inline constexpr unsigned ABISizeOfImage=117084160;
inline constexpr const char* ABIMacroGetterHash="26b8680ef77d01ed3be9db9b0718bb12fa9d16bbc57bd38b6592d618589c6de7";
