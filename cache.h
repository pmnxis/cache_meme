#pragma once

#include <iostream>
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <string>
#include <cmath>
#include <iomanip>

#define WRITE	0x1
#define READ	0x2
#define ISA		0x3

#define HIT		0x0
#define FAIL	0x1
#define UKN		0x2

#define TYPE_FIFO	0x1
#define TYPE_LRU	0x2
#define hex32out(uin32); {std::cout << "0x" <<std::setfill('0') << std::setw(8) << std::hex << uin32;}

#define get_in_idx(input)    ((input >> this->adr_shift) & this->adr_mask)
#define get_in_tag(input)    ((input >> this->tag_shift) & this->tag_mask)
#define get_bl_tag(input)    (input  & this->tag_mask)
#define chk_dirty(input)        (((input>>31)&0x1) == 0x0) 
#define DIRTY	0x00
#define CLEAN	0x01

#define set_dirty(input)        (input |= (0x1 << 31))
#define clr_dirty(input)        (input &= ~(0x1 << 31))
#define get_lru(input)		((input >> this->lru_shift) & this->lru_mask)
// #define set_lru(dst, input)	((dst & ((uint32_t)(lru_mask << this->lru_shift))) | ((input & this->lru_mask) << this->lru_shift) )
#define set_lru(dst, input)	\
dst = (dst & ~(this->lru_mask << this->lru_shift) | (input << this->lru_shift))
int log2data(int data);
int cth(char data);
int atomic_parse(const char cstr[], int * pact, uint32_t * padr);


struct cacheReport {
	int32_t total;
	int16_t block;
	int16_t assoc;
	int32_t access;
	int32_t access_r;
	int32_t access_w;
	// for debug
	int32_t hit;
	// end for debug
	int32_t miss;
	int32_t miss_r;
	int32_t miss_w;
	float ratio;
}typedef cacheReport;

class Cache {
private:
	uint32_t adr_mask;
	uint16_t adr_shift;
	uint32_t tag_mask;
	uint16_t tag_shift;

	uint32_t ** table;
	uint32_t idx_size;

	uint32_t lru_mask;
	uint8_t lru_shift;

	int32_t cache_size;
	int16_t block_size;
	int16_t assoc_val;
	int32_t cycles;
	int rule;
	struct cacheReport report;
public:
	std::string name;
	Cache(const uint32_t total, const uint16_t block, const uint16_t assoc, int rule);
	~Cache();
	int seekItem(const uint32_t adr, const int mode);
	int addItem(const uint32_t adr, const int mode);
	int reLRUtag(const uint32_t adr, const int except, const int hit);
	int rmItem(const uint32_t adr);
	int readItem(const uint32_t adr);
	int writeItem(const uint32_t adr);
	int pollReport(struct cacheReport * rp);
	int parseSimLine(const char * str, int rd_flag, int wr_flag, long nline);
	void printReport();
	void printReport_simple();
	void dbgPrintIdxBlock(const uint32_t adr);
};