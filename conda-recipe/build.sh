# Depending on our platform, shared libraries end with either .so or .dylib
if [[ $(uname) == 'Darwin' ]]; then
    DYLIB_EXT=dylib
    CC=clang
    CXX=clang++
else
    DYLIB_EXT=so
    # Don't specify these -- let conda-build do it.
    #CC=gcc
    #CXX=g++
fi

CONFIGURE_ONLY=0
if [[ $1 != "" ]]; then
    if [[ $1 == "--configure-only" ]]; then
        CONFIGURE_ONLY=1
    else
        echo "Unknown argument: $1"
        exit 1
    fi
fi

# CONFIGURE
mkdir -p build # Using -p here is convenient for calling this script outside of conda.
cd build
cmake ..\
        -DCMAKE_C_COMPILER=${CC} \
        -DCMAKE_CXX_COMPILER=${CXX} \
        -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
        -DCMAKE_PREFIX_PATH="${PREFIX}" \
        -DCMAKE_CXX_FLAGS=-I"${PREFIX}/include" \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-rpath,${PREFIX}/lib -L${PREFIX}/lib" \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath,${PREFIX}/lib -L${PREFIX}/lib" \
        -DBOOST_ROOT=${PREFIX} \
        -DBoost_LIBRARY_DIR="${PREFIX}/lib" \
        -DBoost_INCLUDE_DIR="${PREFIX}/include" \
        -DLIBDVID_INCLUDE="${PREFIX}/include" \
        -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
##

if [[ $CONFIGURE_ONLY == 0 ]]; then
    # BUILD
    make -j${CPU_COUNT}
    
    # "install" to the build prefix (conda will relocate these files afterwards)
    make install
fi
