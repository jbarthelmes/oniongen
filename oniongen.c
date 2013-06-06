/***
Copyright (c) 2012, Johannes Barthelmes
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Johannes Barthelmes nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Johannes Barthelmes BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***/

/* This program has adapted parts of the source code of
   the TOR project (https://www.torproject.org/). The license of the TOR project
   can be found in the TOR-LICENSE file. */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

/* CAREFUL: Linux-specific include. It is used solely for mkdir(3) (see
   `export_private_key'). */
#include <sys/stat.h>

/* These definitions were adapted from the TOR project. */
/* Constants for use with `RSA_generate_key'. For their use, see the OpenSSL
   documentation. */
#define KEYSIZE                    1024
#define PUBLIC_EXPONENT            3

/* The length of the "x" part in "x.onion". */
#define REND_SERVICE_ID_LEN_BASE32 16
#define REND_SERVICE_ID_LEN 10

#define DIGEST_LEN 20

int main(int argc, const char *argv[]);
void generate_service(const char *pattern);
bool pattern_is_not_too_long(const char *pattern);
bool pattern_has_valid_chars(const char *pattern);
void export_private_key(RSA *private_key, const char *pattern);
bool pattern_matches_service_id(const char *pattern, const char *service_id);

/* These functions are adapted from the TOR project. */
bool rend_get_service_id(RSA *pk, char *out);
bool crypto_pk_get_digest(RSA *pk, char *digest_out);
bool crypto_digest(char *digest, const char *m, size_t len);
void base32_encode(char *dest, size_t destlen, const char *src, size_t srclen);


/* Entry point to the application. */
int main(int argc, const char *argv[]) {
  /* Check if there is only one argument (i.e. the pattern to be matched). */
  if (argc == 2) {
    generate_service(argv[1]);
  }
  return 0;
}

/* Try to find a private RSA key that generates a service ID that matches with
   `pattern' by brute-forcing non-deterministically (see also:
   `rend_get_service_id', `pattern_matches_service_id'). */
void generate_service(const char *pattern) {
  /* A pointer to an OpenSSL RSA key structure (see below). */
  RSA *private_key;
  /* A buffer for the generated service ID (see below). Will include
     terminating '\0' character. */
  char service_id[REND_SERVICE_ID_LEN_BASE32 + 1];

  /* Validate the pattern. */
  assert(pattern_is_not_too_long(pattern));
  assert(pattern_has_valid_chars(pattern));

  /* Perform a non-deterministic brute-force search for the key. */
  do {
    /* Generate a random key with the constants from above. */
    private_key = RSA_generate_key(KEYSIZE, PUBLIC_EXPONENT, NULL, NULL);
    assert(private_key != NULL);

    /* Get the service ID that corresponds to `private_key'. */
    assert(rend_get_service_id(private_key, service_id));
  } while (!pattern_matches_service_id(pattern, service_id));

  /* Now that we have a `private_key' that generates a matching `service_id',
     we can export the key to an OpenSSL PEM file. */
  export_private_key(private_key, pattern);
}

/* Check whether `pattern' is not too long. */
bool pattern_is_not_too_long(const char *pattern) {
  /* Obviously, we can't match patterns longer than a service ID. */
  return strlen(pattern) <= REND_SERVICE_ID_LEN_BASE32;
}

/* Check whether the characters of `pattern' are all valid. */
bool pattern_has_valid_chars(const char *pattern) {
  uint8_t i;

  assert(pattern_is_not_too_long(pattern));

  /* "For each character in `pattern'..." */
  for (i = 0; i < strlen(pattern); i++) {
    /* "If `pattern[i]' is not an element of `BASE32_CHARS'..." */
    if (!(islower(pattern[i]) || (pattern[i] >= 2 && pattern[i] <= 7))) {
      return false;
    }
  }

  return true;
}

/* Create an OpenSSL PEM file named "private_key" that contains `private_key'
   in a directory with the name from `pattern'. */
void export_private_key(RSA *private_key, const char *pattern) {
  /* The handle for the OpenSSL PEM file. */
  FILE *pem_file;
  /* The path for `pem_file' relative to the working directory. */
  char *file_relpath;

  const char *file_path = "/private_key";

  /* Create a directory with the name from pattern. */
  assert(mkdir(pattern, S_IRWXU) == 0);

  /* 13 == strlen(file_path) + 1 for the '\0' character. */
  file_relpath = malloc(strlen(pattern)+13);
  assert(file_relpath != NULL);

  strcpy(file_relpath, pattern);
  strcat(file_relpath, file_path);

  pem_file = fopen(file_relpath, "w");
  assert(pem_file != NULL);

  assert(PEM_write_RSAPrivateKey(pem_file, private_key, NULL, NULL, 0, NULL,
         NULL) == 1);

  fclose(pem_file);
  free(file_relpath);
}

bool pattern_matches_service_id(const char *pattern, const char *service_id) {
  return strncmp(pattern, service_id, strlen(pattern)) == 0;
}

/* The following functions have been adapted from the TOR project. */
/* --------------------------------------------------------------- */
bool rend_get_service_id(RSA *pk, char *out) {
  char buf[DIGEST_LEN];

  assert(crypto_pk_get_digest(pk, buf));
  base32_encode(out, REND_SERVICE_ID_LEN_BASE32+1, buf, REND_SERVICE_ID_LEN);

  return true;
}

bool crypto_pk_get_digest(RSA *pk, char *digest_out) {
  unsigned char *buf, *bufp;
  int len;

  len = i2d_RSAPublicKey(pk, NULL);
  assert(len >= 0);

  buf = bufp = malloc(len+1);

  len = i2d_RSAPublicKey(pk, &bufp);
  assert(len >= 0);

  assert(crypto_digest(digest_out, (char*)buf, len));

  free(buf);

  return true;
}

bool crypto_digest(char *digest, const char *m, size_t len) {
  return (SHA1((const unsigned char*)m,len,(unsigned char*)digest) != NULL);
}

void base32_encode(char *dest, size_t destlen, const char *src, size_t srclen) {
  unsigned int i, v, u;
  size_t nbits = srclen * 8, bit;

  const char *BASE32_CHARS = "abcdefghijklmnopqrstuvwxyz234567";

  assert((nbits%5) == 0); /* We need an even multiple of 5 bits. */
  assert((nbits/5)+1 <= destlen); /* We need enough space. */

  for (i=0,bit=0; bit < nbits; ++i, bit+=5) {
    /* set v to the 16-bit value starting at src[bits/8], 0-padded. */
    v = ((uint8_t)src[bit/8]) << 8;
    if (bit+5<nbits) v += (uint8_t)src[(bit/8)+1];
    /* set u to the 5-bit value at the bit'th bit of src. */
    u = (v >> (11-(bit%8))) & 0x1F;
    dest[i] = BASE32_CHARS[u];
  }
  dest[i] = '\0';
}
