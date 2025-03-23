// ================================================================ //
// HUFFPUFF.H, header-file library that handles the DEFLATE-INFLATE //
// operations and provides hi-level wrappers for GZIP file streams, //
// written by Cesar Nicolas-Gonzalez / CNGSOFT during March of 2025 //
// ---------------------------------------------------------------- //
// Requirements: stdio.h (file streams) and string.h (memcpy, etc.) //
// ================================================================ //

// both the DEFLATE/HUFF and INFLATE/PUFF operations handle streams //
// through buffers and will delay error signals as late as possible //

FILE *huffpuff_file=NULL; unsigned char *huffpuff_base,*huffpuff_stop,*huffpuff_here;

unsigned int huffpuff_word,huffpuff_size,huffpuff_scroll;
unsigned char huffpuff_bits,huffpuff_halt;
unsigned char huffpuff_length[352]; // the symbol set is either 288+32+19=339 (HUFF)
unsigned int huffpuff_symbol[352]; // or 16+288+16+32=352 (PUFF), and 352>359, so...

const unsigned int huffpuff_len_k[2][32]={ // length constants; 30 and 31 are unused, 0 is the END MARKER
	{ 0,3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258,0,0 },
	{ 0,0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0,0,0 }};
const unsigned int huffpuff_ofs_k[2][32]={ // offset constants; 30 and 31 are unused
	{ 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0 },
	{ 0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13,0,0 }};
const unsigned char huffpuff_hdr_k[20]={ 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15,0 }; // header constants; 19 is unused

// ================================================================ //
//   huff_XXXX: encode chars into a RFC1951 stream (i.e. DEFLATE)   //
// ================================================================ //

void huff__foo(void) // handle buffer overflow either by writing to the file or setting the error flag
{
	int i=huffpuff_here-huffpuff_base; huffpuff_here=huffpuff_base; if (!huffpuff_halt&&i)
		huffpuff_size+=i,huffpuff_halt=!huffpuff_file||fwrite(huffpuff_base,1,i,huffpuff_file)!=i;
}
void huff_data(char *m,int n) // sends multiple chars to the HUFF stream
{
	for (int i;(i=huffpuff_stop-huffpuff_here)<n;m+=i,n-=i) memcpy(huffpuff_here,m,i),huffpuff_here+=i,huff__foo();
	if (n>0) memcpy(huffpuff_here,m,n),huffpuff_here+=n;
}
void huff_char(unsigned char k) // sends one char to the HUFF stream
	{ if (*huffpuff_here=k,++huffpuff_here>=huffpuff_stop) huff__foo(); }
void huff_bits(unsigned char n,unsigned int k) // sends the "n"-bit integer "k" to the stream
{
	for (k=(k<<huffpuff_bits)|huffpuff_word,n+=huffpuff_bits;n>=8;k>>=8,n-=8) huff_char(k);
	huffpuff_bits=n,huffpuff_word=k;
}
void huff_bit8(void) { if (huffpuff_bits) huff_bits(8-huffpuff_bits,0); }

// The compression level can be either the compile-time constant HUFF_LEVEL
// or a parameter sent to huff_level(); in either case it has ten pre-sets:
// <1 (null compression), 1..8 (quickest..tightest), >8 (full compression).

#define HUFF_RETRY(x) ((x)<1?0:(x)<9?1<<(((x)-1)*2):32768)
#ifdef HUFF_LEVEL
#define huff_retry HUFF_RETRY(HUFF_LEVEL)
#else
int huff_retry=HUFF_RETRY(6); // default level 6!
int huff_level(int z) { return huff_retry=HUFF_RETRY(z),z; }
#endif

unsigned char log2u8(unsigned char x) { unsigned char i=x>=16?x>>=4,4:0; if (x>=4) x>>=2,i+=2; if (x>=2) ++i; return i; } // shorter than log2u16()
unsigned char log2u16(unsigned short x) { unsigned char i=x>=256?x>>=8,8:0; if (x>=16) x>>=4,i+=4; if (x>=4) x>>=2,i+=2; if (x>=2) ++i; return i; }
unsigned short rbit16(unsigned short i) { return i=(i&0X5555)<<1|(i&0XAAAA)>>1,i=(i&0X3333)<<2|(i&0XCCCC)>>2,i=(i&0X0F0F)<<4|(i&0XF0F0)>>4,i<<8|i>>8; }

// The dynamic Huffman encoding requires us to write down the series of commands
// and the symbol frequencies. The maximum width is 15-bit, hence the 32K items.

unsigned int huff_freq[352],huff_item[32768],huff_items;

void huff_item0(void) { memset(huff_freq,huff_items=0,sizeof(huff_freq)); } // reset the item list and its frequencies
void huff_item1(int k) { ++huff_freq[k],huff_item[huff_items++]=k; } // add one literal: k=0..255 (data) or 256 (end marker)
void huff_item2(int m,int r) // add one command: m=3..258 (length), r=1..32768 (offset)
{
	unsigned char m0,r0;
	if (m<11) m0=m-2,m=0; else if (m>=258) m0=29,m=0; else m0=log2u8(m-3)*4-3,m0+=(m-=huffpuff_len_k[0][m0])>>huffpuff_len_k[1][m0];
	if (r< 5) r0=r-1,r=0; else /* no special cases here */ r0=log2u16(r-1)*2 ,r0+=(r-=huffpuff_ofs_k[0][r0])>>huffpuff_ofs_k[1][r0];
	++huff_freq[256+m0],++huff_freq[288+r0],huff_item[huff_items++]=r<<19|r0<<14|r<<9|m0|256; // rrrrrrrr.rrrrrRRR.RRmmmmm1.000MMMMM
}

int huff_init(FILE *f,char *m,int n) // inits HUFF stream; returns zero on OK, nonzero on ERROR
{
	if (n<1) return huffpuff_halt=1;
	huffpuff_stop=(huffpuff_base=huffpuff_here=m)+n; huffpuff_file=f;
	huff_item0(); huffpuff_scroll=0; // reset list table and offset
	return huffpuff_bits=huffpuff_word=huffpuff_size=huffpuff_halt=0;
}

int huff_exit(void) // exits HUFF stream; returns the compressed stream size, or ZERO on error
	{ return huffpuff_halt?0:(huff_bit8(),huff__foo(),huffpuff_halt=1,huffpuff_size); }

// this function performs the actual DEFLATE encoding; it can either encode the entire buffer (buffer,length,0,length)
// or fragments of it, with rising values of "init" and values of "exit" that are below "n", until "exit" matches "n".

int huff_encode(unsigned char *m,int n,int init,int exit)
{
	if (n<1) return 0; // no data! can't do anything!
}

// ================================================================ //
//   puff_XXXX: decode a RFC1951 stream into chars (i.e. INFLATE)   //
// ================================================================ //

int puff_readable;
int puff_init(FILE *f,char *m,int n) // inits PUFF stream; returns zero on OK, nonzero on ERROR
{
	if (n<1) return huffpuff_halt=1;
	huffpuff_base=huffpuff_here=m;
	huffpuff_stop=huffpuff_base+((huffpuff_file=f)?(puff_readable=n,0):n);
	return huffpuff_bits=huffpuff_word=huffpuff_size=huffpuff_halt=0;
}
void puff__foo(void) // handle buffer overflow either by reading from the file or setting the error flag
{
	huffpuff_size+=huffpuff_here-huffpuff_base; huffpuff_here=huffpuff_base; if (!huffpuff_halt)
	{
		int i=huffpuff_file?fread(huffpuff_base,1,puff_readable,huffpuff_file):0;
		huffpuff_stop=(huffpuff_here=huffpuff_base)+i,huffpuff_halt=!i;
	}
}
int puff_data(char *m,int n) // receives multiple chars from the PUFF stream
{
	int o=0; for (int i;(i=huffpuff_stop-huffpuff_here)&&i<n;m+=i,n-=i) o+=i,memcpy(m,huffpuff_here,i),puff__foo();
	if (n>0) memcpy(m,huffpuff_here,n),o+=n,huffpuff_here+=n;
	return o;
}
unsigned char puff_char(void) // receives one char from the PUFF stream
	{ { if (huffpuff_here>=huffpuff_stop) puff__foo(); } return *huffpuff_here++; }
unsigned int puff_bits(unsigned char n) // receives an "n"-bit integer from the stream
{
	unsigned int k=huffpuff_word; while (huffpuff_bits<n) k+=puff_char()<<huffpuff_bits,huffpuff_bits+=8;
	return huffpuff_word=k>>n,huffpuff_bits-=n,k&((1<<n)-1);
}
#define puff_bit8() (huffpuff_bits=0)
int puff_exit(void) // exits PUFF stream; returns the compressed stream size, or ZERO on error
	{ return huffpuff_halt?0:huffpuff_size+huffpuff_here-huffpuff_base; }

// this function performs the actual INFLATE decoding; it can either decode the entire buffer (buffer,length,0,length)
// or fragments of it, with rising values of "init" and values of "exit" that are below "n", until "exit" matches "n".

int puff_decode(unsigned char *m,int n,int init,int exit)
{
	if (n<1) return 0; // no data! can't do anything!
}

// ================================================================ //
//  HUFFPUFF interface for encoding and decoding GZIP file streams  //
// ================================================================ //

int huffpuff_gzip_ofs,huffpuff_gzip_len; char huffpuff_gzip[65536],huffpuff_temp[65536]; // = 32K x2
int huffpuff_gzip_gap; // sliding buffer reference
void huffpuff_gzip_next(void)
{
	memcpy(huffpuff_gzip+    0,huffpuff_gzip+16384,16384);
	memcpy(huffpuff_gzip+16384,huffpuff_gzip+32768,16384);
	memcpy(huffpuff_gzip+32768,huffpuff_gzip+49152,16384);
	huffpuff_scroll-=16384;
}

unsigned int huffpuff_chars,huffpuff_crc32;

// Karl Malbrain's incremental compact CCITT CRC-32
const unsigned int huffpuff_crc32_k[16]={0,0X1DB71064,0X3B6E20C8,0X26D930AC,0X76DC4190,0X6B6B51F4,0X4DB26158,0X5005713C,
	0XEDB88320,0XF00F9344,0XD6D6A3E8,0XCB61B38C,0X9B64C2B0,0X86D3D2D4,0XA00AE278,0XBDBDF21C}; // precalculated table
void huffpuff_crc32_char(unsigned char k)
	{ unsigned int u=huffpuff_crc32^k; u=(u>>4)^huffpuff_crc32_k[u&15],huffpuff_crc32=(u>>4)^huffpuff_crc32_k[u&15]; }
void huffpuff_crc32_data(unsigned char *m,int n)
	{ unsigned int u=huffpuff_crc32; for (huffpuff_chars+=n;n>0;--n) u^=*m++,u=(u>>4)^huffpuff_crc32_k[u&15],u=(u>>4)^huffpuff_crc32_k[u&15]; huffpuff_crc32=u; }
#define huffpuff_gzip_crc32_init() (huffpuff_chars=0,huffpuff_crc32=0XFFFFFFFF) // the first step
#define huffpuff_gzip_crc32_exit() (huffpuff_crc32^=0XFFFFFFFF) // the last step

// ================================================================ //
//       huff_gzip_XXXX: encode chars into a GZIP file stream       //
// ================================================================ //

void huff_gzip__foo(int q) // perform DEFLATE on buffer; "q" nonzero tags the current block as FINAL
{
	#if 0 // STORED!
	if (q&&huffpuff_gzip_len>65535) huff_gzip__foo(0); // avoid the pathological case (64K-1)+1!
	huff_bits(3,q?1:0); huff_bit8();
	int z=huffpuff_gzip_len; if (z>65535) z=65535;
	huff_char(+z),huff_char(+z>>8),huff_char(~z),huff_char(~z>>8);
	huff_data(huffpuff_gzip,z);
	if (!q) memcpy(huffpuff_gzip,huffpuff_gzip+z,huffpuff_gzip_len-=z);
	#elif 0 // STATIC!
	#else // COMPLETE!
	#endif
}

int huff_gzip_init(FILE *f)
{
	if (huff_init(f,huffpuff_temp,65536)) return 0;
	huffpuff_halt=0; huff_data("\037\213\010\000\000\000\000\000\000\000",10);
	huffpuff_gzip_crc32_init();
	huffpuff_gzip_gap=huffpuff_gzip_len=0;
	return !huffpuff_halt;
}
int huff_gzip_send(char *m,int n)
{
	int o=0; while (!huffpuff_halt&&n>0)
	{
		if (huffpuff_gzip_len>=65536) huff_gzip__foo(0);
		int z=65536-huffpuff_gzip_len; if (z>n) z=n;
		memcpy(huffpuff_gzip+huffpuff_gzip_len,m,z);
		huffpuff_gzip_len+=z,huffpuff_crc32_data(m,z);
		m+=z,n-=z,o+=z;
	}
	return o;
}
int huff_gzip_putc(char k)
{
	if (huffpuff_gzip_len>=65536) huff_gzip__foo(0);
	huffpuff_gzip[huffpuff_gzip_len++]=k; huffpuff_crc32_char(k);
	return k;
}
int huff_gzip_exit(void)
{
	if (!huffpuff_halt)
	{
		huffpuff_gzip_crc32_exit();
		char t[8]={huffpuff_crc32,huffpuff_crc32>>8,huffpuff_crc32>>16,huffpuff_crc32>>24,huffpuff_chars,huffpuff_chars>>8,huffpuff_chars>>16,huffpuff_chars>>24};
		huff_gzip__foo(1); huff_data(t,8); huff_exit(); // last data + end words
	}
	return huffpuff_halt?-1:(huffpuff_halt=1,huffpuff_size);
}

// ================================================================ //
//       puff_gzip_XXXX: decode a GZIP file stream into chars       //
// ================================================================ //

void puff_gzip__foo(void) // perform INFLATE on buffer
{
}

int puff_gzip_init(FILE *f)
{
	if (puff_init(f,huffpuff_temp,65536)) return 0;
	if (puff_data(huffpuff_gzip,10)!=10||memcmp(huffpuff_gzip,"\037\213\010",3)) return 0;
	huffpuff_gzip_crc32_init();
	huffpuff_gzip_gap=huffpuff_gzip_len=huffpuff_gzip_ofs=0;
	return 1;
}
int puff_gzip_recv(char *m,int n)
{
	int o=0; while (n>0)
	{
		if (huffpuff_gzip_ofs>=huffpuff_gzip_len) { puff_gzip__foo(); if (!huffpuff_gzip_len) break; }
		int z=huffpuff_gzip_len-huffpuff_gzip_ofs; if (z>n) z=n;
		memcpy(m,huffpuff_gzip+huffpuff_gzip_ofs,z);
		huffpuff_gzip_ofs+=z,huffpuff_crc32_data(m,z);
		m+=z,n-=z,o+=z;
	}
	return o;
}
int puff_gzip_getc(void)
{
	if (huffpuff_gzip_ofs>=huffpuff_gzip_len) { puff_gzip__foo(); if (!huffpuff_gzip_len) return -1; }
	char k=huffpuff_gzip[huffpuff_gzip_ofs++]; huffpuff_crc32_char(k);
	return k;
}
int puff_gzip_exit(void)
{
	//huffpuff_gzip_crc32_exit();
	return 0;
}

// ================================================================ //
// ---------------------------------------------------------------- //
// ================================================================ //
