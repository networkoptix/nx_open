/*
 * File:   logging.h
 * Author: n8
 *
 * Created on June 8, 2018, 4:12 PM
 */

#pragma once

#include <cstdio>

#define debug(...) fprintf(stderr, "[rpi_cam2] " __VA_ARGS__)
#define debugln(s) debug("%s\n", s)
//#define debugln(s)