# manga_real_3d
Allows you to read manga in 3D
## Preview
![](preview.gif)
## Manga structure
First file must be a front cover and last two must be a back cover and a spine
## Installing requirements
```sh
sudo pacman -S sdl3 freeimage glew
```
Also make sure you have [OpenGL support](https://wiki.archlinux.org/title/OpenGL)
## Building
```sh
g++ -lGL -lSDL2 -lfreeimage -lGLEW main.cpp
```
## Running
```sh
./a.out manga_dir ltr
```
Or use `rtl` to read from right to left
```sh
./a.out manga_dir rtl
```
## Navigation
You can rotate the manga book using a mouse with pressed left button. You can zoom in and out using mouse wheel. You can flip the pages using arrows on your keyboard. You can reset the camera using `UP` arrow on your keyboard.
