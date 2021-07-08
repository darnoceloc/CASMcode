# Linux CASM installation from source, assumes you've installed:
#   g++, boost, git, pip, automake, autoconf, libtool, zlib, bash-completion

if [ "$#" -ne 1 ]; then
    echo "Wrong number of arguments. Expected: "
    echo "  install-casm.sh <prefix>"
    exit 1
fi

set -e  # exit if error

# Variables to set:
CASM_REPO="https://github.com/prisms-center/CASMcode.git"  # where to get CASM
CASM_BRANCH="0.3.X"                                        # which branch/tag to build
CASM_PREFIX=$1                                             # where to install CASM
CASM_BOOST_PREFIX=/home/darnoc/anaconda3/envs/casmmc  # where to find boost, often same as CASM_PREFIX
NCPUS=4                         # parallel compilation
BUILD_DIR=/tmp                  # where to clone and build

# clone, configure, make, and install CASM
cd $BUILD_DIR \
  && git clone $CASM_REPO \
  && cd CASMcode \
  && git checkout $CASM_BRANCH \
  && pip install --no-cache-dir six \ # necessary for make_Makemodule.py
  && python make_Makemodule.py \ # create Makemodule.am files
  && ./bootstrap.sh \ # create configure script
  && ./configure \
    CXXFLAGS="-O3 -DNDEBUG -Wno-deprecated-register -Wno-ignored-attributes -Wno-deprecated-declarations" \
    --prefix=/home/darnoc/anaconda3/envs/casmmc \
    --with-boost=-lboost_regex -lboost_filesystem -lboost_program_options -lboost_chrono -lboost_system \
  && make -j $NCPUS \
  && make install
