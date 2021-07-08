# Setup a conda development environment for CASM
set -e
BUILD_SCRIPTS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
export CASM_BUILD_DIR=$(dirname $BUILD_SCRIPTS_DIR)
. $CASM_BUILD_DIR/build_scripts/install-functions.sh

detect_os
check_var "CASM_BUILD_DIR" "CASMcode repository location"
check_var "CASM_CONDA_DIR" "Location to install conda and conda environments" "$HOME/anaconda3"
check_var "CASM_ENV_NAME" "Conda environment name" "casmmc"

. $CASM_BUILD_DIR/build_scripts/build_versions.sh

# install conda
bash $CASM_BUILD_DIR/build_scripts/install-miniconda.sh \
  || { echo "install miniconda failed"; exit 1; }

# install casm development env
bash $CASM_BUILD_DIR/build_scripts/install-env-linux.sh \
  || { echo "create conda environment '$CASM_ENV_NAME' failed"; exit 1; }

. $CASM_CONDA_DIR/etc/profile.d/conda.sh
conda activate $CASM_ENV_NAME

echo ""
echo "The casm conda development environment has been created."
echo "To activate it do:"
echo "  . $CASM_CONDA_DIR/etc/profile.d/conda.sh"
echo "  conda activate $CASM_ENV_NAME"
