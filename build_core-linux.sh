#!/bin/sh
cd bot_core
python2 setup.py build
cd ../
cp -sf bot_core/build/lib.*/bot_core.so bot_core.so