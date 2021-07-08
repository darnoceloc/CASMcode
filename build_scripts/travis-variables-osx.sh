# variables necessary for travis, osx, building in a conda environment

check_var "CASM_PREFIX" "Specify the install location"
check_var "CASM_BOOST_PREFIX" "Specify where boost libraries are installed" "$CASM_PREFIX"
check_var "CASM_ZLIB_PREFIX" "Specify where zlib is installed" "$CASM_PREFIX"

CASM_DEFAULT_CONFIGFLAGS="--prefix=$CASM_PREFIX "
CASM_DEFAULT_CONFIGFLAGS+="--with-zlib=$CASM_ZLIB_PREFIX "
CASM_DEFAULT_CONFIGFLAGS+="--with-boost=$CASM_BOOST_PREFIX "
export CASM_CONFIGFLAGS=${CASM_CONFIGFLAGS:-$CASM_DEFAULT_CONFIGFLAGS}

check_var "CASM_CXXFLAGS" "Compiler flags" "-O3 -Wall -fPIC --std=c++17 -DNDEBUG -fcolor-diagnostics -Wno-deprecated-register -Wno-ignored-attributes -Wno-deprecated-declarations"
check_var "CASM_LDFLAGS" "Linker flags" "-Wl,-rpath,$CASM_BOOST_PREFIX/lib"
check_var "CASM_CC" "C compiler" ${CC:-"cc"}
check_var "CASM_CXX" "C++ compiler" ${CXX:-"c++"}
check_var "CASM_PYTHON" "Python interpreter" ${PYTHON:-"python"}
check_var "CASM_MAKE_OPTIONS" "Options to give 'make'" ""
