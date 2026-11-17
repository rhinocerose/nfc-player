#pragma once

#include <Arduino.h>

void buttonsBegin();
void buttonsLoop();
void buttonsSetHandlers(void (*shortPress)(), void (*longPress)());
