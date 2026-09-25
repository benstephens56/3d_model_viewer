#!/bin/sh
# Builds clipfinder.exe with MSYS2's mingw64 g++ (C:\msys64\mingw64\bin).
# Static, so the exe runs without the MSYS2 DLLs. -ffp-contract=off keeps the
# f32 maths from being fused into multiply-adds, which would change results.
cd "$(dirname "$0")"
GXX="${GXX:-/c/msys64/mingw64/bin/g++.exe}"
PATH="/c/msys64/mingw64/bin:$PATH" "$GXX" -O2 -std=c++17 -ffp-contract=off -static -pthread \
	-o clipfinder.exe clipfinder.cpp
