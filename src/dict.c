#include "fmacroc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>
#include <ctype.h>
#include <assert.h>

#include "dict.h"
#include "zmalloc.h"

static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;

/*---------------------------- hash 函数 -------------------------------*/

unsigned int dictIntHashFunction(unsigned int key)
{
	key += ~(key << 15);
	key ^=  (key >> 10);
	key +=  (key << 3);
	key ^=  (key >> 6);
	key += ~(key << 11);
	key ^=  (key >> 16);
	return key;
}

unsigned int dictIdentityHashFunction(unsigned int key)
{
	return key;
}

static uint32_t dict_hash_function_seed = 5381;

void dictSetHashFunctionSeed(uint32_t seed)
{
	dict_hash_function_seed = seed;
}

uint32_t dictGetHashFunctionSeed(void)
{
	return dict_hash_function_seed;
}

unsigned int dictGenHashFunction(const void *key, int len)
{
	uint32_t seed = dict_hash_function_seed;
	const uint32_t m = 0x5bd1e995;
	const int r = 24;

	uint32_t h = seed ^ len;
	
	const unsigned char *data = (const unsigned char *)key;
	while (len >= 4) {
		uint32_t k = *(uint32_t*)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	switch(len) {
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0]; h *= m;
	};

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return (unsigned int)h;
}

unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len)
{
	unsigned int hash = (unsigned int)dict_hash_function_seed;

	while (len--)
		hash = ((hash << 5) + hash) + (tolower(*buf++)); /* hash * 33 + c */
	return hash;
}


/*---------------------------- 私有函数原型 ----------------------------*/

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static int _dictKeyIndex(dict *ht, const void *key);
static int _dictInit(dict *d, dictType *type, void*privDataPtr);

static void _dictReset(dictht *ht)
{
	ht->table = NULL;
	ht->size = 0;
	ht->sizemask = 0;
	ht->used = 0;
}

/*-------------------------- API ---------------------------------------*/

dict *dictCreate(dictType *type, void *privDataPtr)
{
	dict *d = zmalloc(sizeof(*d));
	
	_dictInit(d, type, privDataPtr);

	return d;
}

int dictResize(dict *d)
{
	int minimal;

	if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;
	minimal = d->ht[0].used;
	if (minimal < DICT_HT_INITIAL_SIZE)
		minimal = DICT_HT_INITIAL_SIZE;

	return dictExpand(d, minimal);
}

int dictExpand(dict *d, unsigned long size)
{
	dictht n;
	unsigned long realsize = _dictNextPower(size);

	if (dictIsRehashing(d) || d->ht[0].used > size)
		return DICT_ERR;

	n.size = realsize;
	n.sizemask = realsize - 1;
	n.table = zmalloc(realsize * sizeof(dictEntry*));
	n.used = 0;

	if (d->ht[0].table == NULL) {
		d->ht[0] = n;
		return DICT_OK;
	}

	d->ht[1] = n;
	d->rehashidx = 0;
	return DICT_OK;
}

int dictRehash(dict *d, int n)
{
	if (!dictIsRehashing(d)) return 0;

	while (n--) {
		dictEntry *de, *nextde;

		if (d->ht[0].used == 0) {
			zfree(d->ht[0].table);
			d->ht[0] = d->ht[1];
			_dictReset(&d->ht[1]);
			d->rehashidx = -1;
			return 0;
		}

		assert(d->ht[0].size > (unsigned long)d->rehashidx);

		while (d->ht[0].table[d->rehashidx] == NULL) d->rehashidx++;
		de = d->ht[0].table[d->rehashidx];

		while (de) {
			unsigned int h;

			nextde = de->next;
			h = dictHashKey(d, de->key) * d->ht[1].sizemask;
			de->next = d->ht[1].table[h];
			d->ht[1].table[h] = de;
			d->ht[0].used--;
			d->ht[1].used++;
			de = nextde;
		}

		d->ht[0].table[d->rehashidx] = NULL;
		d->rehashidx++;
	}

	return 1;
}

long long timeInMilliseconds(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

int dictRehashMilliseconds(dict *d, int ms)
{
	long long start = timeInMilliseconds();
	int rehashes = 0;

	while (dictRehash(d, 100)) {
		rehashes += 100;
		if (timeInMilliseconds() - start > ms) break;
	}

	return rehashes;
}

static int _dictInit(dict *d, dictType *type, void *privDataPtr)
{
	_dictReset(&d->ht[0]);
	_dictReset(&d->ht[1]);
	d->type = type;
	d->privdata = privDataPtr;
	d->rehashidx = -1;
	d->iterators = 0;
	return DICT_OK;
}

static void _dictRehashStep(dict *d)
{
	if (d->iterators == 0) dictRehash(d, 1);
}

int dictAdd(dict *d, void *key, void *val)
{
	dictEntry *entry = dictAddRaw(d, key);
	if (!entry) return DICT_ERR;
	dictSetVal(d, entry, val);
	return DICT_OK;
}

dictEntry *dictAddRaw(dict *d, void *key)
{
	int index;
	dictEntry *entry;
	dictht *ht;

	if (dictIsRehashing(d)) _dictRehashStep(d);

	if ((index = _dictKeyIndex(d, key)) == -1) {
		return NULL;
	}

	ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
	entry = zmalloc(sizeof(*entry));
	entry->next = ht->table[index];
	ht->table[index] = entry;
	ht->used++;

	dictSetKey(d, entry, key);

	return entry;
}

int dictReplace(dict *d, void *key, void *val)
{
	dictEntry *entry, auxentry;
	
	if (dictAdd(d, key, val) == DICT_OK) {
		return 1;
	}	

	entry = dictFind(d, key);

	auxentry = *entry;
	dictSetVal(d, entry, val);
	dictFreeVal(d, &auxentry);
	return 0;
}

dictEntry *dictReplaceRaw(dict *d, void *key)
{
	dictEntry *entry = dictFind(d, key);

	return entry ? entry : dictAddRaw(d, key);
}

static int dictGenericDelete(dict *d, const void *key, int nofree)
{
	unsigned int h, idx;
	dictEntry *he, *prevHe;
	int table;

	if (d->ht[0].size == 0) return DICT_ERR;
	if (dictIsRehashing(d)) _dictRehashStep(d);
	h = dictHashKey(d, key);

	for (table = 0; table <= 1; table++) {
		idx = h & d->ht[table].sizemask;
		he = d->ht[table].table[idx];
		prevHe = NULL;
		while (he) {
			if (dictCompareKeys(d, key, he->key)) {
				if (prevHe) {
					prevHe->next = he->next;
				} else {
					d->ht[table].table[idx] = he->next;
				}

				if (!nofree) {
					dictFreeKey(d, he);
					dictFreeVal(d, he);
				}
				zfree(he);
				d->ht[table].used--;
				return DICT_OK;
			}
			prevHe = he;
			he = he->next;
		}
		if (!dictIsRehashing(d)) break;
	}

	return DICT_ERR;
}

int dictDelete(dict *ht, const void *key)
{
	return dictGenericDelete(ht, key, 0);
}

int dictDeleteNoFree(dict *ht, const void *key)
{
	return dictGenericDelete(ht, key, 1);
}

int _dictClear(dict *d, dictht *ht, void(callback)(void *))
{
	unsigned long i;

	for (i = 0; i < ht->size && ht->used > 0; i++) {
		dictEntry *he, *nextHe;

		if (callback && (i & 65535) == 0) callback(d->privdata);

		if ((he = ht->table[i]) == NULL) continue;
		while (he) {
			nextHe = he->next;
			dictFreeKey(d, he);
			dictFreeVal(d, he);
			zfree(he);
			ht->used--;
			he = nextHe;
		}
	}

	zfree(ht->table);
	_dictReset(ht);

	return DICT_OK;
}

void dictRelease(dict *d)
{
	_dictClear(d, &d->ht[0], NULL);
	_dictClear(d, &d->ht[1], NULL);
	zfree(d);
}

dictEntry *dictFind(dict *d, const void *key)
{
	dictEntry *he;
	unsigned int h, idx, table;

	if (d->ht[0].size == 0) return NULL;
	if (dictIsRehashing(d)) _dictRehashStep(d);
	h = dictHashKey(d, key);
	for (table = 0; table <= 1; table++) {
		idx = h & d->ht[table].sizemask;
		he = d->ht[table].table[idx];
		while(he) {
			if (dictCompareKeys(d, key, he->key)) {
				return he;
			}
			he = he->next;
		}
		if (!dictIsRehashing(d)) return NULL;
	}
	return NULL;
}

void *dictFetchValue(dict *d, const void *key)
{
	dictEntry *he;

	he = dictFind(d, key);
	return he ? dictGetVal(he) : NULL;
}

long long dictFingerprint(dict *d)
{
	long long intergers[6], hash = 0;
	int j;

	intergers[0] = (long) d->ht[0].table;
	intergers[1] = d->ht[0].size;
	intergers[2] = d->ht[0].used;
	intergers[3] = (long) d->ht[1].table;
	intergers[4] = d->ht[1].size;
	intergers[5] = d->ht[1].used;
	
	for (j = 0; j < 6; j++) {
		hash += intergers[j];
		hash = (~hash) + (hash << 21);
		hash = hash ^ (hash >> 24);
		hash = (hash + (hash << 3)) + (hash << 8);
		hash = hash ^ (hash >> 14);
		hash = (hash + (hash << 2)) + (hash << 4);
		hash = hash ^ (hash >> 28);
		hash = hash + (hash << 31);
	}

	return hash;
}

dictIterator *dictGetIterator(dict *d)
{
	dictIterator *iter = zmalloc(sizeof(*iter));

	iter->d = d;
	iter->table = 0;
	iter->index = -1;
	iter->safe = 0;
	iter->entry = NULL;
	iter->nextEntry = NULL;

	return iter;
}

dictIterator *dictGetSafeIterator(dict *d)
{
	dictIterator *i = dictGetIterator(d);

	// 设置安全迭代器标识
	i->safe = 1;

	return i;
}

dictEntry *dictNext(dictIterator *iter)
{
	while(1) {
		if (iter->entry == NULL) {
			dictht *ht = &iter->d->ht[iter->table];
			if (iter->index == -1 && iter->table == 0) {
				if (iter->safe) {
					iter->d->iterators++;
				} else {
					iter->fingerprint = dictFingerprint(iter->d);
				}
			}

			iter->index++;
			if (iter->index >= (long) ht->size) {
				if (dictIsRehashing(iter->d) && iter->table == 0) {
					iter->table++;
					iter->index = 0;
					ht = &iter->d->ht[1];
				} else {
					break;
				}
			}
			iter->entry = ht->table[iter->index];
		} else {
			iter->entry = iter->nextEntry;
		}

		if (iter->entry) {
			iter->nextEntry = iter->entry->next;
			return iter->entry;
		}
	}

	return NULL;
}

void dictReleaseIterator(dictIterator *iter)
{
	if (!(iter->index == -1 && iter->table == 0)) {
		if (iter->safe) {
			iter->d->iterators--;
		} else {
			assert(iter->fingerprint == dictFingerprint(iter->d));
		}
	}

	zfree(iter);
}

dictEntry *dictGetRandomKey(dict *d)
{
	dictEntry *he, *orighe;
	unsigned int h;
	int listlen, listele;

	if (dictSize(d) == 0) return NULL;
	if (dictIsRehashing(d)) _dictRehashStep(d);
	if (dictIsRehashing(d)) {
		do {
			h = random() % (d->ht[0].size + d->ht[1].size);
			he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
										d->ht[0].table[h];
		} while(he == NULL);
	} else {
		do {
			h = random() & d->ht[0].sizemask;
			he = d->ht[0].table[h];
		} while(he == NULL);
	}	

	listlen = 0;
	orighe = he;
	while (he) {
		he = he->next;
		listlen++;
	}
	listele = random() % listlen;
	he = orighe;
	while (listele--) he = he->next;
	return he;
}

int dictGetRandomKeys(dict *d, dictEntry **des, unsigned int count)
{
	int j;
	unsigned int stored = 0;

	if (dictSize(d) < count) count = dictSize(d);
	while(stored < count) {
		for (j = 0; j < 2; j++) {
			unsigned int i = random() & d->ht[j].sizemask;
			int size = d->ht[j].size;

			while (size--) {
				dictEntry *he = d->ht[j].table[i];
				while (he) {
					*des = he;
					des++;
					he = he->next;
					stored++;
					if (stored == count) return stored;
				}
				i = (i + 1) & d->ht[j].sizemask;
			}

			assert(dictIsRehashing(d) != 0);
		}
	}

	return stored;
}

static unsigned long rev(unsigned long v)
{
	unsigned long s = 8 * sizeof(v);
	unsigned long mask = ~0;
	while ((s >> 1) > 0) {
		mask ^= (mask << s);
		v = ((v >> s) & mask) | ((v << s) & ~mask);
	}

	return v;
}

unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata)
{
	dictht *t0, *t1;
	const dictEntry *de;
	unsigned long m0, m1;

	if (dictSize(d) == 0) return 0;

	if (!dictIsRehashing(d)) {
		t0 = &(d->ht[0]);
		m0 = t0->sizemask;
		de = t0->table[v & m0];
		while (de) {
			fn(privdata, de);
			de = de->next;
		}
	} else {
		t0 = &d->ht[0];
		t1 = &d->ht[1];

		if (t0->size > t1->size) {
			t0 = &d->ht[1];
			t1 = &d->ht[0];
		}

		m0 = t0->sizemask;
		m1 = t1->sizemask;
		de = t0->table[v & m0];
		while (de) {
			fn(privdata, de);
			de = de->next;
		}

		do {
			de = t1->table[v & m1];
			while (de) {
				fn(privdata, de);
				de = de->next;
			}

			v = (((v | m0) + 1) & ~m0) | (v & m0);
		} while (v & (m0 ^ m1));
	}

	v |= ~m0;
	v = rev(v);
	v++;
	v = rev(v);

	return v;
}

void dictEmpty(dict *d, void(callback)(void *))
{
	_dictClear(d, &d->ht[0], callback);
	_dictClear(d, &d->ht[1], callback);
	d->rehashidx = -1;
	d->iterators = 0;
}

void dictEnableResize(void)
{
	dict_can_resize = 1;
}

void dictDisableResize(void)
{
	dict_can_resize = 0;
}

/*---------------------------------私有方法---------------------------*/

static int _dictExpandIfNeeded(dict *d)
{
	if (dictIsRehashing(d)) return DICT_OK;

	if (d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);

	if (d->ht[0].used >= d->ht[0].size
		&& (dict_can_resize 
			|| d->ht[0].used / d->ht[0].size > dict_force_resize_ratio)) {
		return dictExpand(d, d->ht[0].used * 2);
	}

	return DICT_OK;
}

static unsigned long _dictNextPower(unsigned long size)
{
	unsigned long i = DICT_HT_INITIAL_SIZE;

	if (size >= LONG_MAX) return LONG_MAX;
	while (1) {
		if (i >= size)
			return i;
		i *= 2;
	}
}

static int _dictKeyIndex(dict *d, const void *key)
{
	unsigned int h, idx, table;
	dictEntry *he;

	if (_dictExpandIfNeeded(d) == DICT_ERR) {
		return -1;
	}	

	h = dictHashKey(d, key);
	for (table = 0; table <= 1; table++) {
		idx = h & d->ht[table].sizemask;
		he = d->ht[table].table[idx];
		while (he) {
			if (dictCompareKeys(d, key, he->key))
				return -1;
			he = he->next;
		}
		if (!dictIsRehashing(d)) break;
	}

	return idx;
}

