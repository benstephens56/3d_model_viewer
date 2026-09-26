#!/bin/sh
# Builds clipfinder.exe from src/ with MSYS2's mingw64 g++ (C:\msys64\mingw64\bin).
# Static, so the exe runs without the MSYS2 DLLs. -ffp-contract=off keeps the
# f32 maths from being fused into multiply-adds, which would change results.
# -flto lets the hot collision functions inline across the source files.
# OUT=path builds somewhere else (e.g. while clipfinder.exe is running).
cd "$(dirname "$0")"
GXX="${GXX:-/c/msys64/mingw64/bin/g++.exe}"
PATH="/c/msys64/mingw64/bin:$PATH" "$GXX" -O2 -std=c++17 -ffp-contract=off -flto=auto -static -pthread \
	-o "${OUT:-clipfinder.exe}" src/*.cpp
