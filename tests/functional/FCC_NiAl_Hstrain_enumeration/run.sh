#!/bin/bash

project_root=/home/darnoc/CASMcode/tests/functional/FCC_NiAl_Hstrain_enumeration
input_files=/home/darnoc/CASMcode/tests/input_files
python_helpers_root=/home/darnoc/CASMcode/tests/functional/FCC_NiAl_Hstrain_enumeration
prim=${input_files}/prims/FCC_NiAl_Hstrain.json

set -e

casm()
{
    /home/darnoc/CASMcode/ccasm "$@"
}

prepare_environment()
{
    cd $project_root
    rm -rf project
    mkdir project
    cd project
}

count_enumerated_configs()
{
    casm status | grep "configurations generated:" | grep -oE '[^[:space:]]+$'
}

count_selected_configs()
{
    casm status | grep "configurations currently selected:" | grep -oE '[^[:space:]]+$'
}

add_json_key_to_file()
{
    jsonstr=$(cat $1)
    jq -c ". + { \"$2\": $3 }" <<<$jsonstr > $1
}

cp_json_value_to_file()
{
    cat $1 $3 | jq -s -c ".[1] + { \"$4\": .[0]$2}" > $3
}

prepare_environment

### Setup: FCC NiAl CASM project ###

# Step 1) Initialize an FCC NiAl CASM projectenumerate supercells up to and including volume 4
cp ${prim} ./prim.json
casm init

# Step 2)  Select composition axes
casm composition --calc > /dev/null
casm composition -s 1 > /dev/null

# Step 3)  Enumerate supercells, to maximum volume 4 (multiples of the prim unit celle)
casm enum -m ScelEnum --max 4 > /dev/null

# Step 4)  Enumerate configurations
# - -m ConfigEnumAllOccupations: all symmetrically distinct occupations
# - --all: in all existing supercells
casm enum -m ConfigEnumAllOccupations --all > /dev/null

# Check: For FCC NiAl this will create exactly 29 configurations
if [ $(count_enumerated_configs) != 29 ]; then
    echo "Expected to have 29 configurations"
    exit 1
fi

# Step 5)  Query properties of the configurations and print to screen

# Select all configurations in MASTER selection (the default selection)
casm select --set-on > /dev/null

# Query:
# - scel_size: supercell size (in multiples of prim unit cell volume)
# - comp_n: number of species per unit cell
# - point_group_name: name of configuration point group
# - multiplicity: number of distinct symmetrically equivalent configurations
casm query -k "scel_size comp_n point_group_name multiplicity"

# By inspection find L12:
#                name  selected  scel_size  comp_n(Ni)  comp_n(Al)  point_group_name  multiplicity
# SCEL4_2_2_1_1_1_0/0         1          4  0.75000000  0.25000000                Oh             1
# SCEL4_2_2_1_1_1_0/1         1          4  0.25000000  0.75000000                Oh             1


### Task: Construct volumetric and deviatoric strained configurations in L12 Ni3Al1 ###

# Step 1) Perform a DoFSpace analysis to find high symmetry strain axes in L12 Ni3Al1
casm sym --dof-space-analysis --dofs Hstrain --confignames SCEL4_2_2_1_1_1_0/0 > /dev/null

# Check: There should be a strain analysis file now
STRAIN_ANALYIS=symmetry/analysis/SCEL4_2_2_1_1_1_0/0/dof_space_Hstrain.json
if [ ! -e $STRAIN_ANALYIS ]; then
    echo "Expected to find $STRAIN_ANALYIS"
    exit 2
fi

# Step 2) Construct an input JSON file for the ConfigEnumStrain method

# Initialize a JSON file
echo "{}" > strain_enum.json

# Copy the high symmetry axes from the DoFSpace analysis file
cp_json_value_to_file $STRAIN_ANALYIS ".irreducible_representations.adapted_axes" strain_enum.json "axes"

# Define the grid on which strained configurations will be sampled
add_json_key_to_file strain_enum.json "min" "[-0.1, -0.1, -0.1, 0, 0, 0]"
add_json_key_to_file strain_enum.json "max" "[0.101, 0.101, 0.101, 0.001, 0.001, 0.001]"
add_json_key_to_file strain_enum.json "increment" "[0.05, 0.05, 0.05, 0.05, 0.05, 0.05]"

# Select the initial enumeration state L12 Ni3Al1
add_json_key_to_file strain_enum.json "confignames" "[\"SCEL4_2_2_1_1_1_0/0\"]"

# Ask to write an output file (default="enum.out") listing enumerated configurations. If false
# (default), the unique new configurations will still be added to the configuration database.
add_json_key_to_file strain_enum.json "output_configurations" "true"

# Trim corners (default=true) excludes all configurations outside of the ellipsoid connecting the
# the extreme points along each exis. Set it to false here to enumerate configurations on the entire
# normal coordinate grid
add_json_key_to_file strain_enum.json "trim_corners" "false"

# Check: Count how many configurations exist in supercell SCEL4_2_2_1_1_1_0
casm select --set 're(scelname,"SCEL4_2_2_1_1_1_0")' > /dev/null
if [ $(count_selected_configs) != 2 ]; then
    echo "Expected to have 3 configurations in SCEL4_2_2_1_1_1_0"
    exit 3
fi

# Check: Count how many configurations exist in other supercells
casm select --set 'not(re(scelname,"SCEL4_2_2_1_1_1_0"))' > /dev/null
if [ $(count_selected_configs) != 27 ]; then
    echo "Excluding SCEL4_2_2_1_1_1_0, expected to have 27 configurations"
    exit 4
fi

# Step 3) Execute the ConfigEnumStrain method
casm enum -m ConfigEnumStrain -s strain_enum.json

# Check: There should be an "enum.out" file listing enumerated configurations
ENUM_OUT=enum.out
if [ ! -e $ENUM_OUT ]; then
    echo "Expected to find $ENUM_OUT"
    exit 5
fi

# Check: There should be 125 (=5*5*5) enumerated configurations, plus a header line
if [ $(wc -l < $ENUM_OUT) != 126 ]; then
    echo "Expected to enumerate 125 configurations"
    exit 6
fi

# Check: Count how many configurations exist in supercell SCEL4_2_2_1_1_1_0 after strain enumeration
casm select --set 're(scelname,"SCEL4_2_2_1_1_1_0")' > /dev/null

# Expect 76 configurations:
# - 2 unstrained configurations
# - 74 symmetrically unique strained configurations
if [ $(count_selected_configs) != 76 ]; then
    echo "Expected to have 76 configurations in SCEL4_2_2_1_1_1_0"
    exit 7
fi

# Check: Count how many configurations exist in other supercells now (should not have changed)
casm select --set 'not(re(scelname,"SCEL4_2_2_1_1_1_0"))' > /dev/null
if [ $(count_selected_configs) != 27 ]; then
    echo "Excluding SCEL4_2_2_1_1_1_0, expected to have 27 configurations"
    exit 8
fi
