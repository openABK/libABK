#!/bin/sh
g++ -I. -I"C:\Boost\include\boost-1_71" -I"../Common" -I"C:\Users\pz\Documents\Git\mfc-tools\Include" sync_client.cpp ../Common/JsonFormatter.cpp -lws2_32 -o sync_client.exe