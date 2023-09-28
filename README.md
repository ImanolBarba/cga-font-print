# cga-rom-print
Simple tool to read a CGA character ROM dump and generate a PNG image with the character set

# Building
Needs libpng:
```
$ clang main.c -o cga_font_print -lpng
```

# Usage
```
$ ./cga_font_print cga_rom.bin out.png
```
Alternatively, one may specify the offset that the program should start reading the ROM from instead of the start, in order to not include invalid character data

```
$ ./cga_font_print cga_rom.bin out.png $((32768 - 4096))
```

# Example output

![out](https://github.com/ImanolBarba/cga-font-print/assets/3193682/830e3352-fde2-4a7c-b388-189149e9ca4b)
