from __future__ import (absolute_import, division, print_function, unicode_literals)
from builtins import *

import argparse
import json
from os import getcwd
from os.path import join, abspath
import sys

from casm.misc import compat, noindent
from casm.project import Project, Selection
import casm.qewrapper
from casm.vaspwrapper import Relax

# casm-calc --configs selection
#           --type "config", "diff_trans", etc.
#           --software "vasp", "quantumexpresso", "sequest" etc. ## shift to calc.json?
#           --method "relax", "neb", etc.
#           --scheduler "pbs"
#           --run / --submit / --setup / --report

configs_help = """
CASM selection file or one of 'CALCULATED', 'ALL', or 'MASTER' (Default)
"""

configtype_help = """
Type of configuartions 'config' (Default), 'diff_trans', 'diff_trans_config' or scel
"""

software_help = """
Warpper for a python package to be used 'vasp' (Default), 'quantumexpressso' or 'sequest'
"""

method_help = """
Calculator type to be used 'relax' (Default) or 'neb'
"""

path_help = """
Path to CASM project. Default=current working directory.
"""

run_help = """
Run calculation for all selected configurations.
"""

submit_help = """
Submit calculation for all selected configurations.
"""

setup_help = """
Setup calculation for all selected configurations.
"""

report_help = """
Report calculation results (print calc.properties.json file) for all selected configurations.
"""
available_calculators = {
  "vasp":{
    "relax": casm.vaspwrapper.Relax,
    "neb": casm.vaspwrapper.Neb
  },
  "quantumexpresso":{
    "relax": casm.qewrapper.Relax,
    "neb": casm.qewrapper.Neb
  }
}

submit_cls = {
  "vasp": casm.vaspwarpper.Submit,
  "quantumexpresso": casm.qewrapper.Submit
}

run_cls = {
  "vasp": casm.vaspwarpper.Run,
  "quantumexpresso": casm.qewrapper.Run
}

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description = 'Submit calculations for CASM')
  parser.add_argument('-c', '--configs', help=configs_help, type=str, default="MASTER")
  parser.add_argument('-t', '--type', help=configtype_help, type=str, default="config")
  parser.add_argument('-w', '--software', help=software_help, type=str, default="vasp")
  parser.add_argument('-m', '--method', help=method_help, type=str, default="relax")
  parser.add_argument('--path', help=path_help, type=str, default=None)
  parser.add_argument('--run', help=run_help, action="store_true", default=False)
  parser.add_argument('--submit', help=submit_help, action="store_true", default=False)
  parser.add_argument('--setup', help=setup_help, action="store_true", default=False)
  parser.add_argument('--report', help=report_help, action="store_true", default=False)
  args = parser.parse_args()
  
  if args.path is None:
    args.path = getcwd()
  
  try:
    proj = Project(abspath(args.path))
    sel = Selection(proj, args.configs, args.type, all=False) 

    # Construct with Selection:
    # - This provides access to the Project, via sel.proj
    # - From the project you can make calls to run interpolation and query lattice relaxations
    calculator = available_calculators[args.software][args.calculator](sel)

    if args.setup:
      calculator.setup()

    elif args.submit:
      calculator.setup()
      submit_cls[args.software](sel, args.methodA)

    elif args.run:
      calculator.setup()
      for configname in sel.data["configname"]:
        relaxation = run_cls[args.software](proj.dir.configuration_dir(configname))
    
    elif args.report:

      #also convert into a class with selection as input
      for configname in sel.data["configname"]:
        configdir = proj.dir.configuration_dir(configname)
        clex = proj.settings.default_clex
        calcdir = proj.dir.calctype_dir(configname, clex)
        finaldir = join(calcdir, "run.final")
        try:
          if args.software == "quantumespresso":
            if settings["outfilename"] is None:
                print("WARNING: No output file specified in relax.json using default outfilename of std.out")
                settings["outfilename"]="std.out"
            outfilename = settings["outfilename"]
            output = casm.qewrapper.Relax.properties(finaldir,outfilename)
          else:
            output = Relax.properties(finaldir)
          calc_props = proj.dir.calculated_properties(configname, clex)
          with open(calc_props, 'w') as file:
            print("writing:", calc_props)
            file.write(six.u(json.dumps(output, cls=NoIndentEncoder, indent=4, sort_keys=True)))
        except:
          print(("Unable to report properties for directory {}.\n" 
                "Please verify that it contains a completed calculation.".format(configdir)))
  except Exception as e:
    print(e)
    sys.exit(1)

if __name__ == "__main__":
  main()
