cd ../_build
make
cd ../test
clang -S -emit-llvm -O0 $1 -o $1.ll
opt -load-pass-plugin=../_build/pass/SVPass.so -passes=super-vectorization $1.ll -S -o out.ll
diff $1.ll out.ll