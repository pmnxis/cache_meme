#include "cache.h"

int log2data(int data) {
	for (int i = 31, t = data; i != 0; i--) {
		if ((t & 0x1 << i) != 0x0) {
			if ((t & ((0x1 << i) - 1)) == 0x0) {
				return i;
			}
			else return (0 - i);
		}
	}
}

int cth(char data) {
	char temp = data & 0xFF;
	if (('0' <= temp) && (temp <= '9')) {
		return temp - '0' + 0x0;
	}
	else if ('A' <= temp && temp <= 'F') {
		return temp - 'A' + 0xA;
	}
	else if ('a' <= temp && temp <= 'f') {
		return temp - 'a' + 0xA;
	}
	else {
		printf("issue %c %x\n", data, data);
		return -1;
	}
}

int atomic_parse(const char cstr[], int * pact, uint32_t * padr) {
	int i,j;
	uint32_t temp = 0;
	char c;
	c = cstr[0];
	*pact = cth(c);
	for (i = strlen(cstr)-3 , j = 0; i >= 0; i--, j++) {
		c = cstr[i+2];
		c = cth(c);
		if ((c == -1) || (c > 0xf)) {
			printf("char to hex - error\n");
			c = 0;
			return -1;
		}else {
			temp |= (c & 0xFF) << (j * 4);
		}
	}
	*padr = temp;
	return 0;
}

Cache::Cache(const uint32_t total, const uint16_t block, const uint16_t assoc, int rule) {
	uint32_t a, b, c, d;
	int t;

	this->cache_size = total;
	this->block_size = block;
	this->assoc_val = assoc;
	this->rule = rule;

	uint32_t idx = this->cache_size / this->block_size / this->assoc_val;
	this->idx_size = idx;
	a = log2data(this->block_size);
	b = idx - 1;

	t = log2data((this->cache_size) / (this->assoc_val));
	if (t<0) {
		c = (uint32_t)(0 - t + 1);
		printf("not good cache size\n");
	}
	else c = (uint32_t)t;
	d = ((uint32_t)~0) >> c;

	this->adr_shift = a;
	this->adr_mask = b;
	this->tag_shift = c;
	this->tag_mask = d;


	// lru masking
	this->rule = rule;
	this->lru_mask = (this->assoc_val - 1);
	this->lru_shift = log2data(this->tag_mask + 1);
	
	// init cache report
	this->report.total = this->cache_size;
	this->report.block = this->block_size;
	this->report.assoc = this->assoc_val;
	report.access = 0;
	report.access_r = 0;
	report.access_w = 0;
	report.miss = 0;
	report.miss_r = 0;
	report.miss_w = 0;
	report.hit = 0;
	report.ratio = 0.0;

	uint16_t i;
	uint32_t j;
	this->table = new uint32_t *[this->assoc_val];
	for (i = 0; i < this->assoc_val; i++) {
		this->table[i] = new uint32_t[this->idx_size];
	}

	for (i = 0; i < this->assoc_val; i++) {
		for (j = 0; j < this->idx_size; j++) {
			this->table[i][j] = 0x1 << 31;
		}
	}
}

Cache::~Cache() {
	uint16_t a;
	for (a = 0; a < this->assoc_val; a++) {
		delete[] this->table[a];
	}
	delete[] this->table;
}

int Cache::seekItem(const uint32_t adr, const int mode) {
	uint32_t idx, tag;
	uint16_t a;
	idx = get_in_idx(adr);
	tag = get_in_tag(adr);
	for (a = 0; a < this->assoc_val; a++) {
		if ( chk_dirty(this->table[a][idx]) == CLEAN && \
			(get_bl_tag(this->table[a][idx]) == tag)
			) {
				return a;
			}
	}
	return -1;
}

int Cache::addItem(const uint32_t adr, const int mode) {
	uint32_t idx, tag;
	int i, t = -1, a, b;
	int k = 0;
	idx = get_in_idx(adr);
	tag = get_in_tag(adr);
	// find same tag match item first

	for (i = 0; i < this->assoc_val; i++) {
		if (get_bl_tag(this->table[i][idx]) == tag) {
			if (chk_dirty(this->table[i][idx]) == DIRTY)continue;
			t = i;
			goto END;
		}
	}

	// find dirty
	if (t == -1) {
		for (i = 0; i < this->assoc_val; i++) {
			if (chk_dirty(this->table[i][idx]) == DIRTY) {
				t = i;
				goto END;
			}
		}
	}

	if (t == -1) {
		a = get_lru(this->table[0][idx]);
		t = 0;
		for (i = 1; i < this->assoc_val; i++) {
			b = get_lru(this->table[i][idx]);
			if (b > a) {
				a = b;
				if (b == (this->assoc_val - 1)) {
					k++;
				}
				t = i;
			}
		}
	}
	if (k > 1)printf("LRU VALUE COHERENCE DETECTED?\n");
END:
	if (t == -1) {
		perror("FIFO or LRU mismatch\n");
		printf("error : "); hex32out(adr);
		printf("\n");
		return -2;
#ifdef DEBUG
		std::cin.get();
#endif // DEBUG
	}
	table[t][idx] = (table[t][idx] & ((this->lru_mask) << (this->lru_shift))) \
		| (tag & this->tag_mask);
	reLRUtag(adr, t, FAIL);
	return t;
}

/*
	const int except : index
	trap : except's index's lru bits

*/
int Cache::reLRUtag(const uint32_t adr, const int except, const int hit) {
	uint32_t idx, tag, a, trap;
	int i;
	int de[32] = { 0, }, k = 0;
	idx = get_in_idx(adr);
	tag = get_in_tag(adr);
	trap = get_lru(this->table[except][idx]);
	if (this->rule == TYPE_LRU) {
		if (hit == HIT) {
			for (i = 0; i < this->assoc_val; i++) {
				if (chk_dirty(this->table[i][idx]) == DIRTY)continue;

				if (i != except) {
					a = get_lru(this->table[i][idx]);
					if (trap > a) {
						if (a < (this->lru_mask))	set_lru(table[i][idx], a + 1);
					}
					a = get_lru(this->table[i][idx]);
					de[a] ++;
				}
				else if (i == except) {
					de[0] ++;
					set_lru(table[i][idx], 0);
				}
			}
		}
		else if (hit == FAIL) {
			for (i = 0; i < this->assoc_val; i++) {
				if (chk_dirty(this->table[i][idx]) == DIRTY)continue;

				if (i != except) {
					a = get_lru(this->table[i][idx]);
					if (a < (this->lru_mask))	set_lru(table[i][idx], a + 1);
					a = get_lru(this->table[i][idx]);
					de[a] ++;
				}
				else if (i == except) {
					de[0] ++;
					set_lru(table[i][idx], 0);
				}
			}
		}

	}
	
	if ((this->rule == TYPE_FIFO)
		) {
		for (i = 0; i < this->assoc_val; i++) {

			if (chk_dirty(this->table[i][idx]) == DIRTY)continue;

			if (i != except) {
				a = get_lru(this->table[i][idx]);
				if (a < (this->lru_mask))	set_lru(table[i][idx], a + 1);
				a = get_lru(this->table[i][idx]);
				de[a] ++;
			}
			else if (i == except) {
				de[0] ++;
				set_lru(table[i][idx], 0);
			}
		}
	}
	for (i = 0; i < this->assoc_val; i++) 
		if (de[i] > 1) {
			k++;
		}

	if (k > 0)
	{
		printf("\n");
		for (i = 0; i < this->assoc_val; i++) {
			printf("de[%d] : %d\n", i, de[i]);

		}
		if (hit == HIT)printf("HIT!\n");
		if (hit == FAIL)printf("FAIL!\n");
		printf("Except : %d\n", except);
		printf("Trap : %d\n\n", trap);
		//dbgPrintIdxBlock(idx);
	}
	return 0;
}

int Cache::rmItem(const uint32_t adr) {
	uint32_t asoc, idx;
	idx = get_in_idx(adr);
	asoc = seekItem(adr, 0);
	if (asoc == -1)return -1;
	this->table[asoc][idx] = 0;
	set_dirty(this->table[asoc][idx]);
	return 0;
}

int Cache::readItem(const uint32_t adr) {
	int a;
	a = seekItem(adr, 0);
	this->report.access++;
	this->report.access_r++;
	if (a >= 0) {
		if (this->rule == TYPE_LRU) {
			reLRUtag(adr, a, HIT);
		}
		this->report.hit++;
		return HIT;
	}
	else if (a == -1) {	
		this->report.miss++;
		this->report.miss_r++;
		addItem(adr, READ);
		return FAIL;
	}
}

int Cache::writeItem(const uint32_t adr) {
	int a;
	a = seekItem(adr, 0);
	this->report.access++;
	this->report.access_w++;
	if (a >= 0) {
		if (this->rule == TYPE_LRU) {
			reLRUtag(adr, a, HIT);
		}
		this->report.hit++;
		return HIT;
	}
	else if (a == -1) {
		this->report.miss++;
		this->report.miss_w++;
		addItem(adr, WRITE);
		return FAIL;
	}
}

int Cache::pollReport(struct cacheReport * rp) {
	this->report.ratio = (float)this->report.miss / (float)this->report.access;
	memcpy(rp, &this->report, sizeof(struct cacheReport));
	return 0;
}

int Cache::parseSimLine(const char * str, int rd_flag, int wr_flag, long nline){
	int act ,rs;
	uint32_t adr;
	rs = atomic_parse(str, &act, &adr);
	if (rs == -1) {
		printf("ParseEr : %ld : %s ,, __padr : ", nline, str); hex32out(adr); printf("\n");
	}
	// this is for debug
	this->cycles++;
	if (act == rd_flag) {
		/*
		if ((get_in_idx(adr) == 0xe37) && (this->rule == TYPE_LRU)) {
			puts(str);
			std::cout << "CALL TAG : 0x" << std::setfill('0') << std::setw(4) << std::hex << get_in_tag(adr) << std::endl;
			dbgPrintIdxBlock(0xe37);
		}
		*/
		return readItem(adr);
	}
	else if (act == wr_flag) {
		/*
		if ((get_in_idx(adr) == 0xe37) && (this->rule == TYPE_LRU)) {
			puts(str);
			std::cout << "CALL TAG : 0x" << std::setfill('0') << std::setw(4) << std::hex << get_in_tag(adr) << std::endl;
			dbgPrintIdxBlock(0xe37);
		}
		*/
		return writeItem(adr);
	}
	else return UKN;
}

void Cache::printReport() {
	std::cout << "--------------------------------------------------------" << std::endl;
	std::cout << "\t"<<this->name << " Cache Simulation Result" << std::endl;
	std::cout << std::endl;
	printf("Cache Size : \t %ld Byte \t : ", this->report.total);
	hex32out(this->report.total);
	printf("\n");

	printf("Block Size : \t %ld Byte \t : ", this->report.block);
	hex32out(this->report.block);
	printf("\n");

	printf("Associtvity: \t %ld Block \t : ", this->report.assoc);
	hex32out(this->report.assoc);
	printf("\n");
	this->report.ratio = (float)this->report.miss / (float)this->report.access;
	std::cout << "---------------- Section Total Behavior ----------------" << std::endl;
	std::cout << "  Access : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.access;
	std::cout << "\t  Ratio  : " << std::setfill(' ') << std::setw(10) << this->report.ratio << std::endl;
	std::cout << "  Miss   : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.miss;
	std::cout << "\t  Hit    : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.hit << std::endl;
	std::cout << "Read Record ------------- Write Record -----------------" << std::endl;
	std::cout << "R_Access : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.access_r << "\t";
	std::cout << "W_Access : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.access_w << std::endl;
	std::cout << "R_Miss   : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.miss_r << "\t";
	std::cout << "W_Miss   : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.miss_w << std::endl;
	std::cout << "R_Ratio  : " << std::setfill(' ') << std::setw(10) << (float)this->report.miss_r / (float)this->report.access_r << "\t";
	std::cout << "W_Ratio  : " << std::setfill(' ') << std::setw(10) << (float)this->report.miss_w / (float)this->report.access_w << std::endl;
	std::cout << "--------------------------------------------------------" << std::endl;
	return;
}

void Cache::printReport_simple() {
	std::cout << this->name << " ";
	std::cout << "\tMiss : " << std::setfill(' ') << std::setw(10) << std::dec << this->report.miss << std::endl;
	return;
}

void Cache::dbgPrintIdxBlock(const uint32_t idx) {
	int i_size = (int)ceil(log2data(this->assoc_val + 1) / 4.0);
	int idx_size = (int)ceil(log2data(this->adr_mask + 1) / 4.0);
	int tag_size = (int)ceil(log2data(this->tag_mask + 1) / 4.0);
	printf("cycles : %d\n", cycles);
	for (uint16_t i = 0; i < this->assoc_val; i++) {
		uint32_t a = this->table[i][idx];
		uint32_t tag = get_bl_tag(a);
		std::cout << "table[0x" << std::setfill('0') << std::setw(i_size) << std::hex << i;
		std::cout << "][0x" << std::setfill('0') << std::setw(idx_size) << std::hex << idx;
		std::cout << "] : ";
		hex32out(a);
		std::cout << " , used : " << (chk_dirty(a));
		std::cout << " tag : 0x" << std::setfill('0') << std::setw(tag_size) << std::hex << get_bl_tag(a);
		std::cout << ", lru : 0x" << std::setfill('0') << std::setw(i_size) << std::hex << get_lru(a) << std::endl;
	}
	//std::cin.get();
}