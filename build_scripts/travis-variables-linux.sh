# variables necessary for travis, linux

check_var "CASM_PREFIX" "Specify the install location"
check_var "CASM_BOOST_PREFIX" "Specify where boost libraries are installed" "$CASM_PREFIX"
check_var "CASM_ZLIB_PREFIX" "Specify where zlib is installed" "$CASM_PREFIX"

CASM_DEFAULT_CONFIGFLAGS="--prefix=$CASM_PREFIX "
CASM_DEFAULT_CONFIGFLAGS+="--with-zlib=$CASM_ZLIB_PREFIX "
CASM_DEFAULT_CONFIGFLAGS+="--with-boost-libdir=$CASM_BOOST_PREFIX/lib "
export CASM_CONFIGFLAGS=${CASM_CONFIGFLAGS:-$CASM_DEFAULT_CONFIGFLAGS}

check_var "CASM_CXXFLAGS" "Compiler flags" "-O3 -Wall -fPIC --std=c++17 -DNDEBUG -Wno-ignored-attributes -Wno-deprecated-declarations -Wno-int-in-bool-context -Wno-sign-compare -Wno-misleading-indentation"
check_var "CASM_LDFLAGS" "Linker flags" ""
check_var "CASM_CC" "C compiler" ${CC:-"cc"} 
check_var "CASM_CXX" "C++ compiler" ${CXX:-"c++"}
check_var "CASM_PYTHON" "Python interpreter" "/home/darnoc/anaconda3/envs/casm_dev/bin/python"
check_var "CASM_MAKE_OPTIONS" "Options to give 'make'" ""
