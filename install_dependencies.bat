cd /D "%~dp0"
cd ./ext
git clone https://github.com/KhronosGroup/glslang.git glslang_repo
cd ./glslang_repo
python update_glslang_sources.py
cmake -DCMAKE_INSTALL_PREFIX="../glslang"
cmake --build . --config Debug --target install
cmake --build . --config Release --target install