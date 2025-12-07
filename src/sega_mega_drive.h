#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
{
	uint16_t connected : 1,
	six_buttons : 1,
	a : 1,
	b : 1,
	c : 1,
	x : 1,
	y : 1,
	z : 1,
	start : 1,
	mode :1,
	up : 1,
	down : 1,
	left : 1,
	right : 1;
} smd_state_t;

void initSegaMegaDrive();
smd_state_t getSegaMegaDriveReport();