#!/bin/bash
set -e

TOOLCHAIN=cmake/ArmToolchain.cmake

echo "========================================="
echo "  Building MASTER (Elevator A + Dispatch)"
echo "========================================="
rm -rf build_master
mkdir -p build_master
cd build_master
cmake .. -DCMAKE_TOOLCHAIN_FILE=../$TOOLCHAIN -DBUILD_MASTER=ON
make -j$(nproc)
cp elevator-master.hex ~/Windows/
cd ..

echo ""
echo "========================================="
echo "  Building SLAVE  (Elevator B)           "
echo "========================================="
rm -rf build_slave
mkdir -p build_slave
cd build_slave
cmake .. -DCMAKE_TOOLCHAIN_FILE=../$TOOLCHAIN -DBUILD_MASTER=OFF
make -j$(nproc)
cp elevator-slave.hex ~/Windows/
cd ..

echo ""
echo "===== BUILD COMPLETE ====="
echo "Master HEX: build_master/elevator-master.hex"
echo "Slave  HEX: build_slave/elevator-slave.hex"