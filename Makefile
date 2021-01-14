all:
        gcc -pthread -o sealTheDeal.out sealTheDeal.c -lm

make clean:
        rm sealTheDeal.out