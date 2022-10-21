/* md2.c - MD2 Message-Digest Algorithm
 * Copyright (C) 2014 Dmitry Eremin-Solenikov
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <config.h>

#include "g10lib.h"
#include "cipher.h"

#include "bithelp.h"
#include "bufhelp.h"
#include "hash-common.h"

typedef struct {
  gcry_md_block_ctx_t bctx;
  unsigned char C[16];
  unsigned char L;
  unsigned char X[48];
} MD2_CONTEXT;

static const unsigned char S[] =
  {
    0x29, 0x2E, 0x43, 0xC9, 0xA2, 0xD8, 0x7C, 0x01,
    0x3D, 0x36, 0x54, 0xA1, 0xEC, 0xF0, 0x06, 0x13,
    0x62, 0xA7, 0x05, 0xF3, 0xC0, 0xC7, 0x73, 0x8C,
    0x98, 0x93, 0x2B, 0xD9, 0xBC, 0x4C, 0x82, 0xCA,
    0x1E, 0x9B, 0x57, 0x3C, 0xFD, 0xD4, 0xE0, 0x16,
    0x67, 0x42, 0x6F, 0x18, 0x8A, 0x17, 0xE5, 0x12,
    0xBE, 0x4E, 0xC4, 0xD6, 0xDA, 0x9E, 0xDE, 0x49,
    0xA0, 0xFB, 0xF5, 0x8E, 0xBB, 0x2F, 0xEE, 0x7A,
    0xA9, 0x68, 0x79, 0x91, 0x15, 0xB2, 0x07, 0x3F,
    0x94, 0xC2, 0x10, 0x89, 0x0B, 0x22, 0x5F, 0x21,
    0x80, 0x7F, 0x5D, 0x9A, 0x5A, 0x90, 0x32, 0x27,
    0x35, 0x3E, 0xCC, 0xE7, 0xBF, 0xF7, 0x97, 0x03,
    0xFF, 0x19, 0x30, 0xB3, 0x48, 0xA5, 0xB5, 0xD1,
    0xD7, 0x5E, 0x92, 0x2A, 0xAC, 0x56, 0xAA, 0xC6,
    0x4F, 0xB8, 0x38, 0xD2, 0x96, 0xA4, 0x7D, 0xB6,
    0x76, 0xFC, 0x6B, 0xE2, 0x9C, 0x74, 0x04, 0xF1,
    0x45, 0x9D, 0x70, 0x59, 0x64, 0x71, 0x87, 0x20,
    0x86, 0x5B, 0xCF, 0x65, 0xE6, 0x2D, 0xA8, 0x02,
    0x1B, 0x60, 0x25, 0xAD, 0xAE, 0xB0, 0xB9, 0xF6,
    0x1C, 0x46, 0x61, 0x69, 0x34, 0x40, 0x7E, 0x0F,
    0x55, 0x47, 0xA3, 0x23, 0xDD, 0x51, 0xAF, 0x3A,
    0xC3, 0x5C, 0xF9, 0xCE, 0xBA, 0xC5, 0xEA, 0x26,
    0x2C, 0x53, 0x0D, 0x6E, 0x85, 0x28, 0x84, 0x09,
    0xD3, 0xDF, 0xCD, 0xF4, 0x41, 0x81, 0x4D, 0x52,
    0x6A, 0xDC, 0x37, 0xC8, 0x6C, 0xC1, 0xAB, 0xFA,
    0x24, 0xE1, 0x7B, 0x08, 0x0C, 0xBD, 0xB1, 0x4A,
    0x78, 0x88, 0x95, 0x8B, 0xE3, 0x63, 0xE8, 0x6D,
    0xE9, 0xCB, 0xD5, 0xFE, 0x3B, 0x00, 0x1D, 0x39,
    0xF2, 0xEF, 0xB7, 0x0E, 0x66, 0x58, 0xD0, 0xE4,
    0xA6, 0x77, 0x72, 0xF8, 0xEB, 0x75, 0x4B, 0x0A,
    0x31, 0x44, 0x50, 0xB4, 0x8F, 0xED, 0x1F, 0x1A,
    0xDB, 0x99, 0x8D, 0x33, 0x9F, 0x11, 0x83, 0x14
};


static void
permute (unsigned char *X, const unsigned char *buf)
{
  int i, j;
  unsigned char t;

  memcpy (X+16, buf, 16);
  for (i = 0; i < 16; i++)
    X[32+i] = X[16+i] ^ X[i];
  t = 0;
  for (i = 0; i < 18; i++)
    {
      for (j = 0; j < 48; j++)
        {
          t = X[j] ^ S[t];
          X[j] = t;
        }
      t += i;
    }
}


static unsigned int
transform_blk (void *c, const unsigned char *data)
{
  MD2_CONTEXT *ctx = c;
  int j;

  for (j = 0; j < 16; j++)
    {
      ctx->C[j] ^= S[data[j] ^ ctx->L];
      ctx->L = ctx->C[j];
    }

  permute(ctx->X, data);

  return /* burn stack */ 4 + 5 * sizeof(void*);
}


static unsigned int
transform ( void *c, const unsigned char *data, size_t nblks )
{
  unsigned int burn;

  do
    {
      burn = transform_blk (c, data);
      data += 64;
    }
  while (--nblks);

  return burn;
}


static void
md2_init (void *context, unsigned int flags)
{
  MD2_CONTEXT *ctx = context;

  (void)flags;

  memset (ctx, 0, sizeof(*ctx));
  ctx->bctx.blocksize_shift = _gcry_ctz(16);
  ctx->bctx.bwrite = transform;
}


static void
md2_final (void *context)
{
  MD2_CONTEXT *hd = context;
  unsigned int burn;

  /* pad */
  memset (hd->bctx.buf + hd->bctx.count,
          16 - hd->bctx.count, 16 - hd->bctx.count);
  burn = transform_blk (hd, hd->bctx.buf);
  permute (hd->X, hd->C);
}

static byte *
md2_read (void *context)
{
  MD2_CONTEXT *hd = (MD2_CONTEXT *) context;
  return hd->X;
}

static const byte asn[18] = /* Object ID is 1.2.840.113549.2.2 */
  { 0x30, 0x20, 0x30, 0x0c, 0x06, 0x08, 0x2a, 0x86,0x48,
    0x86, 0xf7, 0x0d, 0x02, 0x02, 0x05, 0x00, 0x04, 0x10 };

static const gcry_md_oid_spec_t oid_spec_md2[] =
  {
    /* iso.member-body.us.rsadsi.digestAlgorithm.md2 */
    { "1.2.840.113549.2.2" },
    { NULL },
  };

const gcry_md_spec_t _gcry_digest_spec_md2 =
  {
    GCRY_MD_MD2, {0, 0},
    "MD2", asn, DIM (asn), oid_spec_md2, 16,
    md2_init, _gcry_md_block_write, md2_final, md2_read, NULL,
    NULL,
    sizeof (MD2_CONTEXT)
  };
