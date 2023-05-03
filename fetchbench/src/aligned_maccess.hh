#pragma once

#include <utility>
#include "cacheutils.hh"

typedef void (*maccess_func_t)(void*);
using std::pair;

pair<maccess_func_t, maccess_func_t> get_maccess_functions(size_t colliding_bits);

void maccess_5_1(void* p);
void maccess_5_2(void* p);
void maccess_6_1(void* p);
void maccess_6_2(void* p);
void maccess_7_1(void* p);
void maccess_7_2(void* p);
void maccess_8_1(void* p);
void maccess_8_2(void* p);
void maccess_9_1(void* p);
void maccess_9_2(void* p);
void maccess_10_1(void* p);
void maccess_10_2(void* p);
void maccess_11_1(void* p);
void maccess_11_2(void* p);
void maccess_12_1(void* p);
void maccess_12_2(void* p);
void maccess_13_1(void* p);
void maccess_13_2(void* p);
void maccess_14_1(void* p);
void maccess_14_2(void* p);
void maccess_15_1(void* p);
void maccess_15_2(void* p);
void maccess_16_1(void* p);
void maccess_16_2(void* p);
void maccess_17_1(void* p);
void maccess_17_2(void* p);
void maccess_18_1(void* p);
void maccess_18_2(void* p);
void maccess_19_1(void* p);
void maccess_19_2(void* p);
void maccess_20_1(void* p);
void maccess_20_2(void* p);
void maccess_21_1(void* p);
void maccess_21_2(void* p);
void maccess_22_1(void* p);
void maccess_22_2(void* p);
void maccess_23_1(void* p);
void maccess_23_2(void* p);
void maccess_24_1(void* p);
void maccess_24_2(void* p);

void maccess_1(void* p);
void maccess_2(void* p);
void maccess_3(void* p);
void maccess_4(void* p);
void maccess_5(void* p);
void maccess_6(void* p);
void maccess_7(void* p);
void maccess_8(void* p);
void maccess_9(void* p);
void maccess_10(void* p);
void maccess_11(void* p);
void maccess_12(void* p);
void maccess_13(void* p);
void maccess_14(void* p);
void maccess_15(void* p);
void maccess_16(void* p);
void maccess_17(void* p);
void maccess_18(void* p);
void maccess_19(void* p);
void maccess_20(void* p);

