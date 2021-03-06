#ifndef LIGHTNING_LIGHTNINGD_CHAINTOPOLOGY_H
#define LIGHTNING_LIGHTNINGD_CHAINTOPOLOGY_H
#include "config.h"
#include <bitcoin/block.h>
#include <bitcoin/tx.h>
#include <ccan/list/list.h>
#include <ccan/short_types/short_types.h>
#include <ccan/structeq/structeq.h>
#include <ccan/time/time.h>
#include <jsmn.h>
#include <lightningd/watch.h>
#include <stddef.h>

struct bitcoin_tx;
struct bitcoind;
struct command;
struct lightningd;
struct peer;
struct txwatch;

enum feerate {
	FEERATE_IMMEDIATE, /* Aka: aim for next block. */
	FEERATE_NORMAL, /* Aka: next 4 blocks or so. */
	FEERATE_SLOW, /* Aka: next 100 blocks or so. */
};
#define NUM_FEERATES (FEERATE_SLOW+1)

/* Off topology->outgoing_txs */
struct outgoing_tx {
	struct list_node list;
	struct channel *channel;
	const char *hextx;
	struct bitcoin_txid txid;
	void (*failed)(struct channel *channel, int exitstatus, const char *err);
};

struct block {
	int height;

	/* Actual header. */
	struct bitcoin_block_hdr hdr;

	/* Previous block (if any). */
	struct block *prev;

	/* Next block (if any). */
	struct block *next;

	/* Key for hash table */
	struct bitcoin_blkid blkid;

	/* Transactions in this block we care about */
	const struct bitcoin_tx **txs;

	/* And their associated index in the block */
	u32 *txnums;

	/* Full copy of txs (trimmed to txs list in connect_block) */
	struct bitcoin_tx **full_txs;
};

/* Hash blocks by sha */
static inline const struct bitcoin_blkid *keyof_block_map(const struct block *b)
{
	return &b->blkid;
}

static inline size_t hash_sha(const struct bitcoin_blkid *key)
{
	size_t ret;

	memcpy(&ret, key, sizeof(ret));
	return ret;
}

static inline bool block_eq(const struct block *b, const struct bitcoin_blkid *key)
{
	return structeq(&b->blkid, key);
}
HTABLE_DEFINE_TYPE(struct block, keyof_block_map, hash_sha, block_eq, block_map);

struct chain_topology {
	struct block *root;
	struct block *prev_tip, *tip;
	struct block_map block_map;
	u32 feerate[NUM_FEERATES];
	bool startup;

	/* Where to log things. */
	struct log *log;

	/* How far back (in blocks) to go. */
	unsigned int first_blocknum;

	/* How often to poll. */
	struct timerel poll_time;

	/* The bitcoind. */
	struct bitcoind *bitcoind;

	/* Our timer list. */
	struct timers *timers;

	/* Bitcoin transactions we're broadcasting */
	struct list_head outgoing_txs;

	/* Force a particular fee rate regardless of estimatefee (satoshis/kw) */
	u32 *override_fee_rate;

	/* What fee we use if estimatefee fails (satoshis/kw) */
	u32 default_fee_rate;

	/* Transactions/txos we are watching. */
	struct txwatch_hash txwatches;
	struct txowatch_hash txowatches;

#if DEVELOPER
	/* Suppress broadcast (for testing) */
	bool dev_no_broadcast;
#endif
};

/* Information relevant to locating a TX in a blockchain. */
struct txlocator {

	/* The height of the block that includes this transaction */
	u32 blkheight;

	/* Position of the transaction in the transactions list */
	u32 index;
};

/* This is the number of blocks which would have to be mined to invalidate
 * the tx (optional tx is filled in if return is non-zero). */
size_t get_tx_depth(const struct chain_topology *topo,
		    const struct bitcoin_txid *txid,
		    const struct bitcoin_tx **tx);

/* Get highest block number. */
u32 get_block_height(const struct chain_topology *topo);

/* Get fee rate in satoshi per kiloweight. */
u32 get_feerate(const struct chain_topology *topo, enum feerate feerate);

/* Broadcast a single tx, and rebroadcast as reqd (copies tx).
 * If failed is non-NULL, call that and don't rebroadcast. */
void broadcast_tx(struct chain_topology *topo,
		  struct channel *channel, const struct bitcoin_tx *tx,
		  void (*failed)(struct channel *channel,
				 int exitstatus,
				 const char *err));

struct chain_topology *new_topology(struct lightningd *ld, struct log *log);
void setup_topology(struct chain_topology *topology,
		    struct timers *timers,
		    struct timerel poll_time, u32 first_channel_block);

void begin_topology(struct chain_topology *topo);

struct txlocator *locate_tx(const void *ctx, const struct chain_topology *topo, const struct bitcoin_txid *txid);

void notify_new_block(struct lightningd *ld, unsigned int height);
void notify_feerate_change(struct lightningd *ld);

#if DEVELOPER
void json_dev_broadcast(struct command *cmd,
			struct chain_topology *topo,
			const char *buffer, const jsmntok_t *params);

void chaintopology_mark_pointers_used(struct htable *memtable,
				      const struct chain_topology *topo);
#endif
#endif /* LIGHTNING_LIGHTNINGD_CHAINTOPOLOGY_H */
