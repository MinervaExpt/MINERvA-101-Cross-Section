#!/bin/bash
if [ -n "$ROOTSYS" ]; then
  echo "ROOT is already set up.  You must set up the MINERvA 101 tutorial from scratch.  Check your .bash_profile for \"root\""
fi

source /cvmfs/larsoft.opensciencegrid.org/spack-v0.22.0-fermi/setup-env.sh
spack load root@6.28.12 arch=linux-almalinux9-x86_64_v3
spack load gcc@11.4.1
spack load cmake@3.27.9%gcc@11.4.1 arch=linux-almalinux9-x86_64_v3

export LD_LIBRARY_PATH=${ROOTSYS}/lib/root:${LD_LIBRARY_PATH} #Necessary as spack doesn't automatically set this
