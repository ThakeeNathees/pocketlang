
:: A convinent batch script to compile wasm
:: generate pages and run the local server.

cd try
call emsdk_env
python compile.py

cd ..
python generate.py local

cd build
server
