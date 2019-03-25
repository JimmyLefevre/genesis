project_name=genesis
mkdir "../../../build/$project_name" 2> /dev/null
# Compiler flags.
compiler_arguments="-nostdlib -nostartfiles -ffp-contract=fast -fno-autolink -fno-stack-protector -ffreestanding -fno-threadsafe-statics"
# Warning flags...sigh.
compiler_arguments="$compiler_arguments -Weverything -Wno-shift-sign-overflow -Wno-format-security -Wno-unused-parameter -Wno-old-style-cast -Wno-unused-variable -Wno-unused-macros -Wno-newline-eof -Wno-c++11-long-long -Wno-variadic-macros -Wno-null-dereference -Wno-shadow -Wno-gnu-zero-variadic-macro-arguments -Wno-padded -Wno-unused-function -Wno-c++11-extensions -Wno-cast-align -Wno-c++11-compat-deprecated-writable-strings -Wno-pedantic -Wno-undef -Wno-sign-conversion -Wno-undefined-inline -Wno-conversion -Wno-float-equal -Wno-missing-braces -Wno-switch-enum -Wno-c++98-compat -Wno-writable-strings -Wno-c++98-compat-pedantic -Wno-missing-prototypes -Wno-switch -Wno-empty-body -Wno-deprecated -Wno-missing-noreturn -Wno-zero-as-null-pointer-constant -Wno-cast-qual -Wno-int-to-void-pointer-cast"
# Debug flags
compiler_arguments="$compiler_arguments -fstandalone-debug -g"

# Linker flags
# -lGL includes glX functions too.
linker_arguments="-ldl -lX11 -lXi -lGL -lasound -lpthread"

echo "linux_main.cpp"
clang linux_main.cpp -o ../../../build/$project_name/linux_$project_name $compiler_arguments $linker_arguments
echo "game.cpp"
clang game.cpp -o ../../../build/$project_name/$project_name $compiler_arguments -fPIC -shared
