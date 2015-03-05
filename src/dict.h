/*
 * 这个文件实现了一个内存哈希表
 * 哈希表会在表大小的二次方之间进行调整
 * 键冲突通过链表来解决
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

#define DICT_OK 0
#define DICT_ERR 1

#define DICT_NOTUSED(V) ((void)V)

typedef struct dictEntry {
	void *key;
	union {
		void *val;
		uint64_t u64;
		int64_t s64;
		double d;
	} v;
	struct dictEntry *next;
} dictEntry;

typedef struct dictType {
	unsigned int (*hashFunction)(const void *key);
	void *(*keyDup)(void *privdata, const void *key);
	void *(*valDup)(void *privdata, const void *obj);
	int (*keyCompare)(void *privdata, const void *key1, const void *key2);
	void (*keyDestructor)(void *privdata, void *key);
	void (*valDestructor)(void *privdata, void *obj);
} dictType;

typedef struct dictht {
	dictEntry **table;
	unsigned long size;
	unsigned long sizemask;
	unsigned long used;
} dictht;

typedef struct dict {
	dictType *type;
	void *privdata;
	dictht ht[2];
	long rehashidx;
	int iterators;
} dict;

typedef struct dictIterator {
	dict *d;
	long index;
	int table, safe;
	dictEntry *entry, *nextEntry;
	long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

#define DICT_HT_INITIAL_SIZE 4

#define dictFreeVal(d, entry) \
	if ((d)->type->valDestructor) \
		(d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
	if ((d)->type->valDup) \
		entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
	else \
		entry->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
	do { entry->v.s64 = _val_; } while(0)


#define dictSetUnsignedIntegerVal(entry, _val_) \
	do { entry->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
	do { entry->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
	if ((d)->type->keyDestructor) \
		(d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
	if ((d)->type->keyDup) \
		entry->key = (d)->type->keyDup((d)->privdata, _key_); \
	else \
		entry->key = _key_; \
} while(0)

#define dictCompareKeys(d, key1, key2) \
	(((d)->type->keyCompare) ? \
		(d)->type->keyCompare((d)->privdata, key1, key2) : \
		(key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size + (d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used + (d)->ht[1].used)
#define dictIsRehashing(d) ((d)->rehashidx != -1)

dict *dictCreate(dictType *type, void *privDataPtr);
int dictResize(dict *d);
int dictExpand(dict *d, unsigned long size);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key);
dictEntry *dictFind(dict *d, const void *key);
void *dictFetchValue(dict *d, const void *key);
int dictReplace(dict *d, void *key, void *val);
dictEntry *dictReplaceRaw(dict *d, void *key);
int dictDelete(dict *ht, const void *key);
int dictDeleteNoFree(dict *ht, const void *key);
void dictRelease(dict *d);
long long dictFingerprint(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
int dictGetRandomKeys(dict *d, dictEntry **des, unsigned int count);
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata); 
void dictEmpty(dict *d, void(callback)(void *));
void dictEnableResize(void);
void dictDisableResize(void);
unsigned int dictIntHashFunction(unsigned int key);
unsigned int dictIdentityHashFunction(unsigned int key);
void dictSetHashFunctionSeed(uint32_t seed);
uint32_t dictGetHashFunctionSeed(void);
unsigned int dictGenHashFunction(const void *key, int len);
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);

extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif
