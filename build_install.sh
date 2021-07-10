# for running "make" && "make install" for development in a conda environment
# - the conda environment must be activated

### initialization - shouldn't need to touch
set -e
export CASM_BUILD_DIR=/home/darnoc/CASMcode
. $CASM_BUILD_DIR/build_scripts/install-functions.sh
export CASM_OS_NAME=linux
check_var "CONDA_PREFIX" "Must have the conda environment activated"
export CONDA_PREFIX=/home/darnoc/anaconda3/envs/casm_dev
export CASM_PREFIX=$CONDA_PREFIX
export CASM_BOOST_PREFIX=$CONDA_PREFIX

### end initialization ###

### variables - Control how CASM is built  ###

check_var "CASM_CXXFLAGS" "Compiler flags" ""
check_var "CASM_NCPU" "Compiler -j option" 2

# set OS-dependent variable defaults
#   only CASM_CONFIGFLAGS can't be overridden from this script
. $CASM_BUILD_DIR/build_scripts/travis-variables-$CASM_OS_NAME.sh

### end variables ###


bash $CASM_BUILD_DIR/build_scripts/make-cpp.sh

make install
if [[ "$CASM_OS_NAME" == "osx" ]]; then
  echo "install_name_tool -add_rpath $CASM_PREFIX/lib $CASM_PREFIX/bin/ccasm"
  install_name_tool -add_rpath "$CASM_PREFIX/lib" "$CASM_PREFIX/bin/ccasm" \
    || { echo "  already set"; }
fi
pip install casm-python
