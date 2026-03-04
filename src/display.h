#pragma once
#include <Arduino.h>

void displayInit();
void displayUpdate();

// Called by web_server / pump_control to set the status line text.
void displaySetStatus(const String& status);
void displaySetLastAction(const String& action);
