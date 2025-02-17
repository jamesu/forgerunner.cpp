# ForgeRunner

ForgeRunner is an **experimental** Forgejo Actions Runner written in C++.

## Build Instructions

### Prerequisites
Ensure you have the following dependencies installed:
- CMake
- Git
- Google Protobuf libs
- libcurl

### Building the Project

Something like this should suffice:

   ```sh
   mkdir build && cd build
   cmake ..
   make
   ```

## Usage
After building, you can run ForgeRunner using:
```sh
./ForgeRunner --url <url to forgejo instance> --token <registration token from forgejo> --label myworker
```

## License
ForgeRunner is licensed under the GPLV3 License. See the [LICENSE](LICENSE) file for details.

## Disclaimer
This project is **experimental** and not yet ready for production use. Use at your own risk!

