# electrumz

Install deps:
`vcpkg --triplet x64-windows install libuv lmdb spdlog zeromq argtable2 mbedtls nlohmann-json`

Build:
```
	mkdir build && cd build
	cmake .. -A x64
```

To run test electrum: 
`electrum.exe --regtest -s localhost:5555:t`