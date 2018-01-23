#!/bin/sh

# CLEAN
if [ "$1" = "clean" ]; then
    sudo make clean
fi

# BUILD QT
if [ "$1" = "qt" ]; then
    cd ~/com/raze/razeblock
    ./configure -shared -opensource -nomake examples -nomake tests -confirm-license -prefix
    make
    make install
fi

# BUILD RAZE
if [ "$1" = "build" ]; then
    cd ~/com/raze/razeblock
    git submodule update --init --recursive
    cmake -G "Unix Makefiles" -DRAIBLOCKS_GUI=ON -DBOOST_ROOT="$BOOST_ROOT"
    #make rai_node
    make rai_wallet
    cp rai_wallet .. && cp librai_lib.so  .. && cd .. && ./rai_wallet
fi

# BOOST
if [ "$1" = "boost" ]; then
    sh ci/bootstrap_boost.sh
fi

# FIX BOOST (UBUNTU)
if [ "$1" = "fix" ]; then
    cd /home/carlo/opt/boost_1_66_0

    export BOOST_ROOT=/usr/local
    ./bootstrap.sh "--prefix=$BOOST_ROOT"

    export BOOST_BUILD=$HOME/opt/boost_1_66_0.BUILD
    user_configFile=`find $PWD -name user-config.jam`
    echo "using mpi ;" >> $user_configFile
    n=`cat /proc/cpuinfo | grep "cpu cores" | uniq | awk '{print $NF}'`

    sudo ./b2 --with=all --prefix=$BOOST_ROOT --build-dir=$BOOST_BUILD -j $n install
    sudo ldconfig
fi
