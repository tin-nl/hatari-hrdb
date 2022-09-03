source "/home/me/emsdk/emsdk_env.sh"
cd build
emmake cmake -DCPUEMU_68000_ONLY=1 -DEMSCRIPTEN=true -DENABLE_DSP_EMU=0 -DENABLE_TRACING=0 -DCMAKE_SYSTEM_NAME=Linux ..
emmake make -j2 hatari
