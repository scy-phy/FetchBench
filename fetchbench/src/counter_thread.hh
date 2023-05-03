#pragma once

#include <unistd.h>
#include <thread>
#include "utils.hh"

extern size_t volatile ctr_thread_ctr;
void ctr_thread_start(int cpu);
void ctr_thread_stop();