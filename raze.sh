#!/bin/sh

# CONFIG
BACK="$PWD"
CORE="~/com/raze/razeblock"
BOOST_DL="~/opt/boost_1_66_0"

# CLEAN
if [ "$1" = "clean" ]; then
    cd "$CORE"
    sudo make clean
    echo "Project Build Assets Removed Successfully"
fi

# BUILD QT
if [ "$1" = "qt" ]; then
    cd "$CORE"

    ./configure -shared -opensource -nomake examples -nomake tests -confirm-license -prefix
    make
    sudo make install

    echo "QT Assets Built Successfully"
fi

# BUILD RAZE
if [ "$1" = "build" ]; then
    cd "$CORE"

    git submodule update --init --recursive
    export BOOST_ROOT=/usr/local
    cmake -G "Unix Makefiles" -DRAZE_GUI=ON -DBOOST_ROOT="$BOOST_ROOT"
    make raze_node
    make raze_wallet

    cp raze_wallet .. && cp raze_node .. && cp libraze_lib.so .. && ../raze_wallet
    echo "Project Built Successfully"
fi

# BOOST IMPLEMENTATION
if [ "$1" = "boost" ]; then
    cd "$CORE"
    sh ci/bootstrap_boost.sh
    echo "Boost Implemented Successfully"
fi

# FIX BOOST (UBUNTU/MINT)
if [ "$1" = "fix" ]; then
    cd "$BOOST_DL"

    export BOOST_ROOT=/usr/local
    ./bootstrap.sh "--prefix=$BOOST_ROOT"
    export BOOST_BUILD="$BOOST_DL".BUILD
    user_configFile=`find $PWD -name user-config.jam`
    echo "using mpi ;" >> $user_configFile

    n=`cat /proc/cpuinfo | grep "cpu cores" | uniq | awk '{print $NF}'`
    sudo ./b2 --with=all --prefix=$BOOST_ROOT --build-dir=$BOOST_BUILD -j $n install
    sudo ldconfig

    echo "Boost Setup Fixed Successfully"
fi
