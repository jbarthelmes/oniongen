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

Donate
------

I accept donations via Bitcoin: `1DQkWHtzVqwt6v3C1JEr5i34ypkNGBM9tC`.
Everything is appreciated! :heart:
