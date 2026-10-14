/* Host test for decompress_image(): build and run with tests/run.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <decompress.h>

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } else printf("ok:   %s\n", msg); } while (0)

static unsigned char *load(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	unsigned char *b;

	if (!f) { perror(path); exit(2); }
	fseek(f, 0, SEEK_END); *len = ftell(f); rewind(f);
	b = malloc(*len);
	if (fread(b, 1, *len, f) != *len) exit(2);
	fclose(f);
	return b;
}

/* Output buffer with a PROT_NONE page right behind it: any write past the
 * end of the buffer faults. */
static unsigned char *guarded(size_t size)
{
	size_t page = 4096, total = ((size + page - 1) / page) * page;
	unsigned char *p = mmap(0, total + page, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	mprotect(p + total, page, PROT_NONE);
	/* place the buffer so that it ends exactly at the guard page */
	return p + total - size;
}

int main(int argc, char **argv)
{
	size_t clen, rlen, n;
	unsigned char *comp, *raw, *out;
	unsigned char *syn;

	if (argc < 3) { fprintf(stderr, "usage: %s u-boot.lzf u-boot-raw.bin\n", argv[0]); return 2; }
	comp = load(argv[1], &clen);
	raw = load(argv[2], &rlen);

	/* 1: real image, generous limit, exact result */
	out = guarded(1 << 20);
	n = decompress_image(out, comp, clen, 1 << 20);
	CHECK(n == rlen && memcmp(out, raw, rlen) == 0, "real u-boot image round trip");

	/* 2: limit exactly equal to the size passes, one byte less fails */
	out = guarded(rlen);
	n = decompress_image(out, comp, clen, rlen);
	CHECK(n == rlen && memcmp(out, raw, rlen) == 0, "limit == image size");
	out = guarded(rlen - 1);
	n = decompress_image(out, comp, clen, rlen - 1);
	CHECK(n == 0, "limit == image size - 1 rejected");

	/* 3: XMODEM padding (up to 127 bytes) after the last block is accepted */
	{
		unsigned char *p = calloc(1, clen + 127);

		memcpy(p, comp, clen);
		memset(p + clen, 0x1a, 127);
		out = guarded(1 << 20);
		n = decompress_image(out, p, clen + 127, 1 << 20);
		CHECK(n == rlen && memcmp(out, raw, rlen) == 0, "127 bytes XMODEM padding accepted");
	}

	/* 4: bad header byte */
	{
		unsigned char *p = malloc(clen);

		memcpy(p, comp, clen);
		p[2] = 7;
		out = guarded(1 << 20);
		n = decompress_image(out, p, clen, 1 << 20);
		CHECK(n == 0, "bad block type rejected");
	}

	/* 5: synthetic stream of raw blocks bigger than the limit (1 MiB + 64 KiB) */
	{
		size_t blocks = 17, bsz = 65535, i, pos = 0;

		syn = malloc(blocks * (5 + bsz));
		for (i = 0; i < blocks; i++) {
			syn[pos++] = 'Z'; syn[pos++] = 'V'; syn[pos++] = 0;
			syn[pos++] = bsz >> 8; syn[pos++] = bsz & 0xff;
			memset(syn + pos, 0xA5, bsz); pos += bsz;
		}
		out = guarded(1 << 20);
		n = decompress_image(out, syn, pos, 1 << 20);
		CHECK(n == 0, "stream larger than the 1 MiB limit rejected, no write past buffer");
	}

	/* 6: input cut inside a block: must be rejected without reading past the
	 * end of the (exactly sized) input buffer */
	{
		size_t cut = clen / 2;
		unsigned char *p = malloc(cut);

		memcpy(p, comp, cut);
		out = guarded(1 << 20);
		n = decompress_image(out, p, cut, 1 << 20);
		CHECK(n == 0, "input truncated inside a block rejected");
		free(p);
	}

	/* 7: input cut inside a type 1 header (6 of 7 header bytes present) */
	{
		unsigned char *p = malloc(6);

		memcpy(p, comp, 6);
		out = guarded(1 << 20);
		n = decompress_image(out, p, 6, 1 << 20);
		CHECK(n == 0 || n == rlen * 0, "truncated header does not read out of bounds");
		free(p);
	}

	/* 8: raw (type 0) block whose length field exceeds the input */
	{
		unsigned char *p = malloc(5 + 10);

		p[0] = 'Z'; p[1] = 'V'; p[2] = 0; p[3] = 0x10; p[4] = 0x00;   /* claims 4096 bytes */
		memset(p + 5, 0xA5, 10);
		out = guarded(1 << 20);
		n = decompress_image(out, p, 15, 1 << 20);
		CHECK(n == 0, "raw block longer than the input rejected");
		free(p);
	}

	printf(fails ? "RESULT: %d FAILED\n" : "RESULT: all passed\n", fails);
	return fails != 0;
}
