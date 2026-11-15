#pragma once
#include <Arduino.h>

void   netBegin();
void   netUpdate();
void   netApply();            // перечитать настройки WiFi (вкл/выкл, STA)
bool   netWifiActive();
bool   netStaConnected();
String netApIp();
String netStaIp();
int    netClients();
int    netRssi();
