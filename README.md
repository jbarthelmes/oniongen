oniongen
========

Generate vanity .onion URLs (i.e. they match a pattern you specify) with your CPU.

Compiling
---------

Compile with any C compiler, link with libcrypto.

Example:

```bash
$ gcc oniongen.c -o oniongen -lcrypto
```
  
Usage
-----

This will find an .onion URL for you that begins with "onion".
  
```bash
$ ./oniongen onion
```
