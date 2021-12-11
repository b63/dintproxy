Building
-----

Clone the repository and install all needed dependencies, which will
mostly include
  - cmake >= 3.8
  - make
  - Crypto++
  - [dint](https://github.com/b63/dint)

The quick-and-dirty way to get Crypto++ and dint is to first
clone and build dint locally. Then add the following symoblic link
to the root directory of this project. Assuming dint was cloned in
the same parent directory,
```bash
ln -s ../dint/include dint
ln -s ../dint/bin/libdint.a lib/
ln -s ../dint/lib/cryptopp  lib/
```

Then the project can be build by running,
```bash
cmake -Bbuild && cmake --build build
```

After the build is finished, the proxy-server/proxy-client binaries 
will be found under `bin/`.

Options for running `proxy-client` and `proxy-server`.
```shell
$ bin/proxy-client --help
proxy-server [--lname=<local name>] [--lport=<local port>]
    [--sname=<server name>] [--sport=<server port>]

$ bin/proxy-server --help
proxy-server [--name=<local name>] [--port=<local port>]
```

By default, the local proxy listens on loopback interface (127.0.0.1) port 5001
and looks for remote proxy on same interface port 8001.
