# package installation
## Windows
 ```.\vcpkg install glfw3:x64-windows glew:x64-windows tiff:x64-windows```

## Ubuntu
 ```sudo apt install libglfw3-dev libglew-dev libtiff-dev```

 # Compile
 ```
mkdir build
cd build
cmake ..
make
```

# How to use
In the build directory, do 

```
./viewer ./path/to/image0 ./path/to/image1 ...
```

Supports multiple images 

By pressing ← → , change image
