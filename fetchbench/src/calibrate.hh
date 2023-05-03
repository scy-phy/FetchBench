#pragma once
#include <cinttypes>
#include <unistd.h>

size_t access_measure(uint8_t* ptr1, uint8_t* ptr2, size_t repeat, size_t median);
void calibrate(size_t& fr_thresh, size_t& noise_thresh, int& use_nanosleep);