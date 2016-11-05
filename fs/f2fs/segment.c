/*
 * fs/f2fs/segment.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/fs.h>
#include <linux/f2fs_fs.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/prefetch.h>
<<<<<<< HEAD
#include <linux/kthread.h>
#include <linux/swap.h>
#include <linux/timer.h>
=======
#include <linux/vmalloc.h>
>>>>>>> 512ca3c... stock

#include "f2fs.h"
#include "segment.h"
#include "node.h"
<<<<<<< HEAD
#include "trace.h"
#include <trace/events/f2fs.h>

#define __reverse_ffz(x) __reverse_ffs(~(x))

static struct kmem_cache *discard_entry_slab;
static struct kmem_cache *sit_entry_set_slab;
static struct kmem_cache *inmem_entry_slab;

/**
 * Copied from latest lib/llist.c
 * llist_for_each_entry_safe - iterate over some deleted entries of
 *                             lock-less list of given type
 *			       safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @node:	the first entry of deleted list entries.
 * @member:	the name of the llist_node with the struct.
 *
 * In general, some entries of the lock-less list can be traversed
 * safely only after being removed from list, so start with an entry
 * instead of list head.
 *
 * If being used on entries deleted from lock-less list directly, the
 * traverse order is from the newest to the oldest added entry.  If
 * you want to traverse from the oldest to the newest, you must
 * reverse the order by yourself before traversing.
 */
#define llist_for_each_entry_safe(pos, n, node, member)			       \
	for (pos = llist_entry((node), typeof(*pos), member);		       \
		&pos->member != NULL &&					       \
		(n = llist_entry(pos->member.next, typeof(*n), member), true); \
		pos = n)

/**
 * Copied from latest lib/llist.c
 * llist_reverse_order - reverse order of a llist chain
 * @head:	first item of the list to be reversed
 *
 * Reverse the order of a chain of llist entries and return the
 * new first entry.
 */
struct llist_node *llist_reverse_order(struct llist_node *head)
{
	struct llist_node *new_head = NULL;

	while (head) {
		struct llist_node *tmp = head;
		head = head->next;
		tmp->next = new_head;
		new_head = tmp;
	}

	return new_head;
}

/**
 * Copied from latest linux/list.h
 * list_last_entry - get the last element from a list
 * @ptr:        the list head to take the element from.
 * @type:       the type of the struct this is embedded in.
 * @member:     the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)

static unsigned long __reverse_ulong(unsigned char *str)
{
	unsigned long tmp = 0;
	int shift = 24, idx = 0;

#if BITS_PER_LONG == 64
	shift = 56;
#endif
	while (shift >= 0) {
		tmp |= (unsigned long)str[idx++] << shift;
		shift -= BITS_PER_BYTE;
	}
	return tmp;
}

/*
 * __reverse_ffs is copied from include/asm-generic/bitops/__ffs.h since
 * MSB and LSB are reversed in a byte by f2fs_set_bit.
 */
static inline unsigned long __reverse_ffs(unsigned long word)
{
	int num = 0;

#if BITS_PER_LONG == 64
	if ((word & 0xffffffff00000000UL) == 0)
		num += 32;
	else
		word >>= 32;
#endif
	if ((word & 0xffff0000) == 0)
		num += 16;
	else
		word >>= 16;

	if ((word & 0xff00) == 0)
		num += 8;
	else
		word >>= 8;

	if ((word & 0xf0) == 0)
		num += 4;
	else
		word >>= 4;

	if ((word & 0xc) == 0)
		num += 2;
	else
		word >>= 2;

	if ((word & 0x2) == 0)
		num += 1;
	return num;
}

/*
 * __find_rev_next(_zero)_bit is copied from lib/find_next_bit.c because
 * f2fs_set_bit makes MSB and LSB reversed in a byte.
 * @size must be integral times of unsigned long.
 * Example:
 *                             MSB <--> LSB
 *   f2fs_set_bit(0, bitmap) => 1000 0000
 *   f2fs_set_bit(7, bitmap) => 0000 0001
 */
static unsigned long __find_rev_next_bit(const unsigned long *addr,
			unsigned long size, unsigned long offset)
{
	const unsigned long *p = addr + BIT_WORD(offset);
	unsigned long result = size;
	unsigned long tmp;

	if (offset >= size)
		return size;

	size -= (offset & ~(BITS_PER_LONG - 1));
	offset %= BITS_PER_LONG;

	while (1) {
		if (*p == 0)
			goto pass;

		tmp = __reverse_ulong((unsigned char *)p);

		tmp &= ~0UL >> offset;
		if (size < BITS_PER_LONG)
			tmp &= (~0UL << (BITS_PER_LONG - size));
		if (tmp)
			goto found;
pass:
		if (size <= BITS_PER_LONG)
			break;
		size -= BITS_PER_LONG;
		offset = 0;
		p++;
	}
	return result;
found:
	return result - size + __reverse_ffs(tmp);
}

static unsigned long __find_rev_next_zero_bit(const unsigned long *addr,
			unsigned long size, unsigned long offset)
{
	const unsigned long *p = addr + BIT_WORD(offset);
	unsigned long result = size;
	unsigned long tmp;

	if (offset >= size)
		return size;

	size -= (offset & ~(BITS_PER_LONG - 1));
	offset %= BITS_PER_LONG;

	while (1) {
		if (*p == ~0UL)
			goto pass;

		tmp = __reverse_ulong((unsigned char *)p);

		if (offset)
			tmp |= ~0UL << (BITS_PER_LONG - offset);
		if (size < BITS_PER_LONG)
			tmp |= ~0UL >> size;
		if (tmp != ~0UL)
			goto found;
pass:
		if (size <= BITS_PER_LONG)
			break;
		size -= BITS_PER_LONG;
		offset = 0;
		p++;
	}
	return result;
found:
	return result - size + __reverse_ffz(tmp);
}

void register_inmem_page(struct inode *inode, struct page *page)
{
	struct f2fs_inode_info *fi = F2FS_I(inode);
	struct inmem_pages *new;

	f2fs_trace_pid(page);

	set_page_private(page, (unsigned long)ATOMIC_WRITTEN_PAGE);
	SetPagePrivate(page);

	new = f2fs_kmem_cache_alloc(inmem_entry_slab, GFP_NOFS);

	/* add atomic page indices to the list */
	new->page = page;
	INIT_LIST_HEAD(&new->list);

	/* increase reference count with clean state */
	mutex_lock(&fi->inmem_lock);
	get_page(page);
	list_add_tail(&new->list, &fi->inmem_pages);
	inc_page_count(F2FS_I_SB(inode), F2FS_INMEM_PAGES);
	mutex_unlock(&fi->inmem_lock);

	trace_f2fs_register_inmem_page(page, INMEM);
}

static int __revoke_inmem_pages(struct inode *inode,
				struct list_head *head, bool drop, bool recover)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	struct inmem_pages *cur, *tmp;
	int err = 0;

	list_for_each_entry_safe(cur, tmp, head, list) {
		struct page *page = cur->page;

		if (drop)
			trace_f2fs_commit_inmem_page(page, INMEM_DROP);

		lock_page(page);

		if (recover) {
			struct dnode_of_data dn;
			struct node_info ni;

			trace_f2fs_commit_inmem_page(page, INMEM_REVOKE);

			set_new_dnode(&dn, inode, NULL, NULL, 0);
			if (get_dnode_of_data(&dn, page->index, LOOKUP_NODE)) {
				err = -EAGAIN;
				goto next;
			}
			get_node_info(sbi, dn.nid, &ni);
			f2fs_replace_block(sbi, &dn, dn.data_blkaddr,
					cur->old_addr, ni.version, true, true);
			f2fs_put_dnode(&dn);
		}
next:
		ClearPageUptodate(page);
		set_page_private(page, 0);
		ClearPageUptodate(page);
		f2fs_put_page(page, 1);

		list_del(&cur->list);
		kmem_cache_free(inmem_entry_slab, cur);
		dec_page_count(F2FS_I_SB(inode), F2FS_INMEM_PAGES);
	}
	return err;
}

void drop_inmem_pages(struct inode *inode)
{
	struct f2fs_inode_info *fi = F2FS_I(inode);

	mutex_lock(&fi->inmem_lock);
	__revoke_inmem_pages(inode, &fi->inmem_pages, true, false);
	mutex_unlock(&fi->inmem_lock);
}

static int __commit_inmem_pages(struct inode *inode,
					struct list_head *revoke_list)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	struct f2fs_inode_info *fi = F2FS_I(inode);
	struct inmem_pages *cur, *tmp;
	struct f2fs_io_info fio = {
		.sbi = sbi,
		.type = DATA,
		.rw = WRITE_SYNC | REQ_PRIO,
		.encrypted_page = NULL,
	};
	bool submit_bio = false;
	int err = 0;

	list_for_each_entry_safe(cur, tmp, &fi->inmem_pages, list) {
		struct page *page = cur->page;

		lock_page(page);
		if (page->mapping == inode->i_mapping) {
			trace_f2fs_commit_inmem_page(page, INMEM);

			set_page_dirty(page);
			f2fs_wait_on_page_writeback(page, DATA, true);
			if (clear_page_dirty_for_io(page))
				inode_dec_dirty_pages(inode);

			fio.page = page;
			err = do_write_data_page(&fio);
			if (err) {
				unlock_page(page);
				break;
			}

			/* record old blkaddr for revoking */
			cur->old_addr = fio.old_blkaddr;

			clear_cold_data(page);
			submit_bio = true;
		}
		unlock_page(page);
		list_move_tail(&cur->list, revoke_list);
	}

	if (submit_bio)
		f2fs_submit_merged_bio_cond(sbi, inode, NULL, 0, DATA, WRITE);

	if (!err)
		__revoke_inmem_pages(inode, revoke_list, false, false);

	return err;
}

int commit_inmem_pages(struct inode *inode)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	struct f2fs_inode_info *fi = F2FS_I(inode);
	struct list_head revoke_list;
	int err;

	INIT_LIST_HEAD(&revoke_list);
	f2fs_balance_fs(sbi, true);
	f2fs_lock_op(sbi);

	mutex_lock(&fi->inmem_lock);
	err = __commit_inmem_pages(inode, &revoke_list);
	if (err) {
		int ret;
		/*
		 * try to revoke all committed pages, but still we could fail
		 * due to no memory or other reason, if that happened, EAGAIN
		 * will be returned, which means in such case, transaction is
		 * already not integrity, caller should use journal to do the
		 * recovery or rewrite & commit last transaction. For other
		 * error number, revoking was done by filesystem itself.
		 */
		ret = __revoke_inmem_pages(inode, &revoke_list, false, true);
		if (ret)
			err = ret;

		/* drop all uncommitted pages */
		__revoke_inmem_pages(inode, &fi->inmem_pages, true, false);
	}
	mutex_unlock(&fi->inmem_lock);

	f2fs_unlock_op(sbi);
	return err;
}

=======
#include <trace/events/f2fs.h>

>>>>>>> 512ca3c... stock
/*
 * This function balances dirty node and dentry pages.
 * In addition, it controls garbage collection.
 */
<<<<<<< HEAD
void f2fs_balance_fs(struct f2fs_sb_info *sbi, bool need)
{
	if (!need)
		return;
=======
void f2fs_balance_fs(struct f2fs_sb_info *sbi)
{
>>>>>>> 512ca3c... stock
	/*
	 * We should do GC or end up with checkpoint, if there are so many dirty
	 * dir/node pages without enough free segments.
	 */
	if (has_not_enough_free_secs(sbi, 0)) {
		mutex_lock(&sbi->gc_mutex);
<<<<<<< HEAD
		f2fs_gc(sbi, false);
	}
}

void f2fs_balance_fs_bg(struct f2fs_sb_info *sbi)
{
	/* try to shrink extent cache when there is no enough memory */
	if (!available_free_memory(sbi, EXTENT_CACHE))
		f2fs_shrink_extent_tree(sbi, EXTENT_CACHE_SHRINK_NUMBER);

	/* check the # of cached NAT entries */
	if (!available_free_memory(sbi, NAT_ENTRIES))
		try_to_free_nats(sbi, NAT_ENTRY_PER_BLOCK);

	if (!available_free_memory(sbi, FREE_NIDS))
		try_to_free_nids(sbi, NAT_ENTRY_PER_BLOCK * FREE_NID_PAGES);

	/* checkpoint is the only way to shrink partial cached entries */
	if (!available_free_memory(sbi, NAT_ENTRIES) ||
			!available_free_memory(sbi, INO_ENTRIES) ||
			excess_prefree_segs(sbi) ||
			excess_dirty_nats(sbi) ||
			(is_idle(sbi) && f2fs_time_over(sbi, CP_TIME))) {
		if (test_opt(sbi, DATA_FLUSH)) {
			struct blk_plug plug;

			blk_start_plug(&plug);
			sync_dirty_inodes(sbi, FILE_INODE);
			blk_finish_plug(&plug);
		}
		f2fs_sync_fs(sbi->sb, true);
		stat_inc_bg_cp_count(sbi->stat_info);
	}
}

static int issue_flush_thread(void *data)
{
	struct f2fs_sb_info *sbi = data;
	struct flush_cmd_control *fcc = SM_I(sbi)->cmd_control_info;
	wait_queue_head_t *q = &fcc->flush_wait_queue;
repeat:
	if (kthread_should_stop())
		return 0;

	if (!llist_empty(&fcc->issue_list)) {
		struct bio *bio;
		struct flush_cmd *cmd, *next;
		int ret;

		bio = f2fs_bio_alloc(0);

		fcc->dispatch_list = llist_del_all(&fcc->issue_list);
		fcc->dispatch_list = llist_reverse_order(fcc->dispatch_list);

		bio->bi_bdev = sbi->sb->s_bdev;
		ret = submit_bio_wait(WRITE_FLUSH, bio);

		llist_for_each_entry_safe(cmd, next,
					  fcc->dispatch_list, llnode) {
			cmd->ret = ret;
			complete(&cmd->wait);
		}
		bio_put(bio);
		fcc->dispatch_list = NULL;
	}

	wait_event_interruptible(*q,
		kthread_should_stop() || !llist_empty(&fcc->issue_list));
	goto repeat;
}

int f2fs_issue_flush(struct f2fs_sb_info *sbi)
{
	struct flush_cmd_control *fcc = SM_I(sbi)->cmd_control_info;
	struct flush_cmd cmd;

	trace_f2fs_issue_flush(sbi->sb, test_opt(sbi, NOBARRIER),
					test_opt(sbi, FLUSH_MERGE));

	if (test_opt(sbi, NOBARRIER))
		return 0;

	if (!test_opt(sbi, FLUSH_MERGE)) {
		struct bio *bio = f2fs_bio_alloc(0);
		int ret;

		bio->bi_bdev = sbi->sb->s_bdev;
		ret = submit_bio_wait(WRITE_FLUSH, bio);
		bio_put(bio);
		return ret;
	}

	init_completion(&cmd.wait);

	llist_add(&cmd.llnode, &fcc->issue_list);

	if (!fcc->dispatch_list)
		wake_up(&fcc->flush_wait_queue);

	wait_for_completion(&cmd.wait);

	return cmd.ret;
}

int create_flush_cmd_control(struct f2fs_sb_info *sbi)
{
	dev_t dev = sbi->sb->s_bdev->bd_dev;
	struct flush_cmd_control *fcc;
	int err = 0;

	fcc = kzalloc(sizeof(struct flush_cmd_control), GFP_KERNEL);
	if (!fcc)
		return -ENOMEM;
	init_waitqueue_head(&fcc->flush_wait_queue);
	init_llist_head(&fcc->issue_list);
	SM_I(sbi)->cmd_control_info = fcc;
	fcc->f2fs_issue_flush = kthread_run(issue_flush_thread, sbi,
				"f2fs_flush-%u:%u", MAJOR(dev), MINOR(dev));
	if (IS_ERR(fcc->f2fs_issue_flush)) {
		err = PTR_ERR(fcc->f2fs_issue_flush);
		kfree(fcc);
		SM_I(sbi)->cmd_control_info = NULL;
		return err;
	}

	return err;
}

void destroy_flush_cmd_control(struct f2fs_sb_info *sbi)
{
	struct flush_cmd_control *fcc = SM_I(sbi)->cmd_control_info;

	if (fcc && fcc->f2fs_issue_flush)
		kthread_stop(fcc->f2fs_issue_flush);
	kfree(fcc);
	SM_I(sbi)->cmd_control_info = NULL;
=======
		f2fs_gc(sbi);
	}
>>>>>>> 512ca3c... stock
}

static void __locate_dirty_segment(struct f2fs_sb_info *sbi, unsigned int segno,
		enum dirty_type dirty_type)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);

	/* need not be added */
	if (IS_CURSEG(sbi, segno))
		return;

	if (!test_and_set_bit(segno, dirty_i->dirty_segmap[dirty_type]))
		dirty_i->nr_dirty[dirty_type]++;

	if (dirty_type == DIRTY) {
		struct seg_entry *sentry = get_seg_entry(sbi, segno);
<<<<<<< HEAD
		enum dirty_type t = sentry->type;

		if (unlikely(t >= DIRTY)) {
			f2fs_bug_on(sbi, 1);
			return;
		}
		if (!test_and_set_bit(segno, dirty_i->dirty_segmap[t]))
			dirty_i->nr_dirty[t]++;
=======
		enum dirty_type t = DIRTY_HOT_DATA;

		dirty_type = sentry->type;

		if (!test_and_set_bit(segno, dirty_i->dirty_segmap[dirty_type]))
			dirty_i->nr_dirty[dirty_type]++;

		/* Only one bitmap should be set */
		for (; t <= DIRTY_COLD_NODE; t++) {
			if (t == dirty_type)
				continue;
			if (test_and_clear_bit(segno, dirty_i->dirty_segmap[t]))
				dirty_i->nr_dirty[t]--;
		}
>>>>>>> 512ca3c... stock
	}
}

static void __remove_dirty_segment(struct f2fs_sb_info *sbi, unsigned int segno,
		enum dirty_type dirty_type)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);

	if (test_and_clear_bit(segno, dirty_i->dirty_segmap[dirty_type]))
		dirty_i->nr_dirty[dirty_type]--;

	if (dirty_type == DIRTY) {
<<<<<<< HEAD
		struct seg_entry *sentry = get_seg_entry(sbi, segno);
		enum dirty_type t = sentry->type;

		if (test_and_clear_bit(segno, dirty_i->dirty_segmap[t]))
			dirty_i->nr_dirty[t]--;
=======
		enum dirty_type t = DIRTY_HOT_DATA;

		/* clear all the bitmaps */
		for (; t <= DIRTY_COLD_NODE; t++)
			if (test_and_clear_bit(segno, dirty_i->dirty_segmap[t]))
				dirty_i->nr_dirty[t]--;
>>>>>>> 512ca3c... stock

		if (get_valid_blocks(sbi, segno, sbi->segs_per_sec) == 0)
			clear_bit(GET_SECNO(sbi, segno),
						dirty_i->victim_secmap);
	}
}

/*
 * Should not occur error such as -ENOMEM.
 * Adding dirty entry into seglist is not critical operation.
 * If a given segment is one of current working segments, it won't be added.
 */
<<<<<<< HEAD
static void locate_dirty_segment(struct f2fs_sb_info *sbi, unsigned int segno)
=======
void locate_dirty_segment(struct f2fs_sb_info *sbi, unsigned int segno)
>>>>>>> 512ca3c... stock
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	unsigned short valid_blocks;

	if (segno == NULL_SEGNO || IS_CURSEG(sbi, segno))
		return;

	mutex_lock(&dirty_i->seglist_lock);

	valid_blocks = get_valid_blocks(sbi, segno, 0);

	if (valid_blocks == 0) {
		__locate_dirty_segment(sbi, segno, PRE);
		__remove_dirty_segment(sbi, segno, DIRTY);
	} else if (valid_blocks < sbi->blocks_per_seg) {
		__locate_dirty_segment(sbi, segno, DIRTY);
	} else {
		/* Recovery routine with SSR needs this */
		__remove_dirty_segment(sbi, segno, DIRTY);
	}

	mutex_unlock(&dirty_i->seglist_lock);
<<<<<<< HEAD
}

static int f2fs_issue_discard(struct f2fs_sb_info *sbi,
				block_t blkstart, block_t blklen)
{
	sector_t start = SECTOR_FROM_BLOCK(blkstart);
	sector_t len = SECTOR_FROM_BLOCK(blklen);
	struct seg_entry *se;
	unsigned int offset;
	block_t i;

	for (i = blkstart; i < blkstart + blklen; i++) {
		se = get_seg_entry(sbi, GET_SEGNO(sbi, i));
		offset = GET_BLKOFF_FROM_SEG0(sbi, i);

		if (!f2fs_test_and_set_bit(offset, se->discard_map))
			sbi->discard_blks--;
	}
	trace_f2fs_issue_discard(sbi->sb, blkstart, blklen);
	return blkdev_issue_discard(sbi->sb->s_bdev, start, len, GFP_NOFS, 0);
}

bool discard_next_dnode(struct f2fs_sb_info *sbi, block_t blkaddr)
{
	int err = -EOPNOTSUPP;

	if (test_opt(sbi, DISCARD)) {
		struct seg_entry *se = get_seg_entry(sbi,
				GET_SEGNO(sbi, blkaddr));
		unsigned int offset = GET_BLKOFF_FROM_SEG0(sbi, blkaddr);

		if (f2fs_test_bit(offset, se->discard_map))
			return false;

		err = f2fs_issue_discard(sbi, blkaddr, 1);
	}

	if (err) {
		update_meta_page(sbi, NULL, blkaddr);
		return true;
	}
	return false;
}

static void __add_discard_entry(struct f2fs_sb_info *sbi,
		struct cp_control *cpc, struct seg_entry *se,
		unsigned int start, unsigned int end)
{
	struct list_head *head = &SM_I(sbi)->discard_list;
	struct discard_entry *new, *last;

	if (!list_empty(head)) {
		last = list_last_entry(head, struct discard_entry, list);
		if (START_BLOCK(sbi, cpc->trim_start) + start ==
						last->blkaddr + last->len) {
			last->len += end - start;
			goto done;
		}
	}

	new = f2fs_kmem_cache_alloc(discard_entry_slab, GFP_NOFS);
	INIT_LIST_HEAD(&new->list);
	new->blkaddr = START_BLOCK(sbi, cpc->trim_start) + start;
	new->len = end - start;
	list_add_tail(&new->list, head);
done:
	SM_I(sbi)->nr_discards += end - start;
}

static void add_discard_addrs(struct f2fs_sb_info *sbi, struct cp_control *cpc)
{
	int entries = SIT_VBLOCK_MAP_SIZE / sizeof(unsigned long);
	int max_blocks = sbi->blocks_per_seg;
	struct seg_entry *se = get_seg_entry(sbi, cpc->trim_start);
	unsigned long *cur_map = (unsigned long *)se->cur_valid_map;
	unsigned long *ckpt_map = (unsigned long *)se->ckpt_valid_map;
	unsigned long *discard_map = (unsigned long *)se->discard_map;
	unsigned long *dmap = SIT_I(sbi)->tmp_map;
	unsigned int start = 0, end = -1;
	bool force = (cpc->reason == CP_DISCARD);
	int i;

	if (se->valid_blocks == max_blocks)
		return;

	if (!force) {
		if (!test_opt(sbi, DISCARD) || !se->valid_blocks ||
		    SM_I(sbi)->nr_discards >= SM_I(sbi)->max_discards)
			return;
	}

	/* SIT_VBLOCK_MAP_SIZE should be multiple of sizeof(unsigned long) */
	for (i = 0; i < entries; i++)
		dmap[i] = force ? ~ckpt_map[i] & ~discard_map[i] :
				(cur_map[i] ^ ckpt_map[i]) & ckpt_map[i];

	while (force || SM_I(sbi)->nr_discards <= SM_I(sbi)->max_discards) {
		start = __find_rev_next_bit(dmap, max_blocks, end + 1);
		if (start >= max_blocks)
			break;

		end = __find_rev_next_zero_bit(dmap, max_blocks, start + 1);
		__add_discard_entry(sbi, cpc, se, start, end);
	}
}

void release_discard_addrs(struct f2fs_sb_info *sbi)
{
	struct list_head *head = &(SM_I(sbi)->discard_list);
	struct discard_entry *entry, *this;

	/* drop caches */
	list_for_each_entry_safe(entry, this, head, list) {
		list_del(&entry->list);
		kmem_cache_free(discard_entry_slab, entry);
	}
=======
	return;
>>>>>>> 512ca3c... stock
}

/*
 * Should call clear_prefree_segments after checkpoint is done.
 */
static void set_prefree_as_free_segments(struct f2fs_sb_info *sbi)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
<<<<<<< HEAD
	unsigned int segno;

	mutex_lock(&dirty_i->seglist_lock);
	for_each_set_bit(segno, dirty_i->dirty_segmap[PRE], MAIN_SEGS(sbi))
		__set_test_and_free(sbi, segno);
	mutex_unlock(&dirty_i->seglist_lock);
}

void clear_prefree_segments(struct f2fs_sb_info *sbi, struct cp_control *cpc)
{
	struct list_head *head = &(SM_I(sbi)->discard_list);
	struct discard_entry *entry, *this;
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	unsigned long *prefree_map = dirty_i->dirty_segmap[PRE];
	unsigned int start = 0, end = -1;

	mutex_lock(&dirty_i->seglist_lock);

	while (1) {
		int i;
		start = find_next_bit(prefree_map, MAIN_SEGS(sbi), end + 1);
		if (start >= MAIN_SEGS(sbi))
			break;
		end = find_next_zero_bit(prefree_map, MAIN_SEGS(sbi),
								start + 1);

		for (i = start; i < end; i++)
			clear_bit(i, prefree_map);

		dirty_i->nr_dirty[PRE] -= end - start;

		if (!test_opt(sbi, DISCARD))
			continue;

		f2fs_issue_discard(sbi, START_BLOCK(sbi, start),
				(end - start) << sbi->log_blocks_per_seg);
	}
	mutex_unlock(&dirty_i->seglist_lock);

	/* send small discards */
	list_for_each_entry_safe(entry, this, head, list) {
		if (cpc->reason == CP_DISCARD && entry->len < cpc->trim_minlen)
			goto skip;
		f2fs_issue_discard(sbi, entry->blkaddr, entry->len);
		cpc->trimmed += entry->len;
skip:
		list_del(&entry->list);
		SM_I(sbi)->nr_discards -= entry->len;
		kmem_cache_free(discard_entry_slab, entry);
	}
}

static bool __mark_sit_entry_dirty(struct f2fs_sb_info *sbi, unsigned int segno)
{
	struct sit_info *sit_i = SIT_I(sbi);

	if (!__test_and_set_bit(segno, sit_i->dirty_sentries_bitmap)) {
		sit_i->dirty_sentries++;
		return false;
	}

	return true;
=======
	unsigned int segno, offset = 0;
	unsigned int total_segs = TOTAL_SEGS(sbi);

	mutex_lock(&dirty_i->seglist_lock);
	while (1) {
		segno = find_next_bit(dirty_i->dirty_segmap[PRE], total_segs,
				offset);
		if (segno >= total_segs)
			break;
		__set_test_and_free(sbi, segno);
		offset = segno + 1;
	}
	mutex_unlock(&dirty_i->seglist_lock);
}

void clear_prefree_segments(struct f2fs_sb_info *sbi)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	unsigned int segno, offset = 0;
	unsigned int total_segs = TOTAL_SEGS(sbi);

	mutex_lock(&dirty_i->seglist_lock);
	while (1) {
		segno = find_next_bit(dirty_i->dirty_segmap[PRE], total_segs,
				offset);
		if (segno >= total_segs)
			break;

		offset = segno + 1;
		if (test_and_clear_bit(segno, dirty_i->dirty_segmap[PRE]))
			dirty_i->nr_dirty[PRE]--;

		/* Let's use trim */
		if (test_opt(sbi, DISCARD))
			blkdev_issue_discard(sbi->sb->s_bdev,
					START_BLOCK(sbi, segno) <<
					sbi->log_sectors_per_block,
					1 << (sbi->log_sectors_per_block +
						sbi->log_blocks_per_seg),
					GFP_NOFS, 0);
	}
	mutex_unlock(&dirty_i->seglist_lock);
}

static void __mark_sit_entry_dirty(struct f2fs_sb_info *sbi, unsigned int segno)
{
	struct sit_info *sit_i = SIT_I(sbi);
	if (!__test_and_set_bit(segno, sit_i->dirty_sentries_bitmap))
		sit_i->dirty_sentries++;
>>>>>>> 512ca3c... stock
}

static void __set_sit_entry_type(struct f2fs_sb_info *sbi, int type,
					unsigned int segno, int modified)
{
	struct seg_entry *se = get_seg_entry(sbi, segno);
	se->type = type;
	if (modified)
		__mark_sit_entry_dirty(sbi, segno);
}

static void update_sit_entry(struct f2fs_sb_info *sbi, block_t blkaddr, int del)
{
	struct seg_entry *se;
	unsigned int segno, offset;
	long int new_vblocks;

	segno = GET_SEGNO(sbi, blkaddr);

	se = get_seg_entry(sbi, segno);
	new_vblocks = se->valid_blocks + del;
<<<<<<< HEAD
	offset = GET_BLKOFF_FROM_SEG0(sbi, blkaddr);

	f2fs_bug_on(sbi, (new_vblocks >> (sizeof(unsigned short) << 3) ||
=======
	offset = GET_SEGOFF_FROM_SEG0(sbi, blkaddr) & (sbi->blocks_per_seg - 1);

	BUG_ON((new_vblocks >> (sizeof(unsigned short) << 3) ||
>>>>>>> 512ca3c... stock
				(new_vblocks > sbi->blocks_per_seg)));

	se->valid_blocks = new_vblocks;
	se->mtime = get_mtime(sbi);
	SIT_I(sbi)->max_mtime = se->mtime;

	/* Update valid block bitmap */
	if (del > 0) {
<<<<<<< HEAD
		if (f2fs_test_and_set_bit(offset, se->cur_valid_map))
			f2fs_bug_on(sbi, 1);
		if (!f2fs_test_and_set_bit(offset, se->discard_map))
			sbi->discard_blks--;
	} else {
		if (!f2fs_test_and_clear_bit(offset, se->cur_valid_map))
			f2fs_bug_on(sbi, 1);
		if (f2fs_test_and_clear_bit(offset, se->discard_map))
			sbi->discard_blks++;
=======
		if (f2fs_set_bit(offset, se->cur_valid_map))
			BUG();
	} else {
		if (!f2fs_clear_bit(offset, se->cur_valid_map))
			BUG();
>>>>>>> 512ca3c... stock
	}
	if (!f2fs_test_bit(offset, se->ckpt_valid_map))
		se->ckpt_valid_blocks += del;

	__mark_sit_entry_dirty(sbi, segno);

	/* update total number of valid blocks to be written in ckpt area */
	SIT_I(sbi)->written_valid_blocks += del;

	if (sbi->segs_per_sec > 1)
		get_sec_entry(sbi, segno)->valid_blocks += del;
}

<<<<<<< HEAD
void refresh_sit_entry(struct f2fs_sb_info *sbi, block_t old, block_t new)
{
	update_sit_entry(sbi, new, 1);
	if (GET_SEGNO(sbi, old) != NULL_SEGNO)
		update_sit_entry(sbi, old, -1);

	locate_dirty_segment(sbi, GET_SEGNO(sbi, old));
	locate_dirty_segment(sbi, GET_SEGNO(sbi, new));
=======
static void refresh_sit_entry(struct f2fs_sb_info *sbi,
			block_t old_blkaddr, block_t new_blkaddr)
{
	update_sit_entry(sbi, new_blkaddr, 1);
	if (GET_SEGNO(sbi, old_blkaddr) != NULL_SEGNO)
		update_sit_entry(sbi, old_blkaddr, -1);
>>>>>>> 512ca3c... stock
}

void invalidate_blocks(struct f2fs_sb_info *sbi, block_t addr)
{
	unsigned int segno = GET_SEGNO(sbi, addr);
	struct sit_info *sit_i = SIT_I(sbi);

<<<<<<< HEAD
	f2fs_bug_on(sbi, addr == NULL_ADDR);
=======
	BUG_ON(addr == NULL_ADDR);
>>>>>>> 512ca3c... stock
	if (addr == NEW_ADDR)
		return;

	/* add it into sit main buffer */
	mutex_lock(&sit_i->sentry_lock);

	update_sit_entry(sbi, addr, -1);

	/* add it into dirty seglist */
	locate_dirty_segment(sbi, segno);

	mutex_unlock(&sit_i->sentry_lock);
}

<<<<<<< HEAD
bool is_checkpointed_data(struct f2fs_sb_info *sbi, block_t blkaddr)
{
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned int segno, offset;
	struct seg_entry *se;
	bool is_cp = false;

	if (blkaddr == NEW_ADDR || blkaddr == NULL_ADDR)
		return true;

	mutex_lock(&sit_i->sentry_lock);

	segno = GET_SEGNO(sbi, blkaddr);
	se = get_seg_entry(sbi, segno);
	offset = GET_BLKOFF_FROM_SEG0(sbi, blkaddr);

	if (f2fs_test_bit(offset, se->ckpt_valid_map))
		is_cp = true;

	mutex_unlock(&sit_i->sentry_lock);

	return is_cp;
}

=======
>>>>>>> 512ca3c... stock
/*
 * This function should be resided under the curseg_mutex lock
 */
static void __add_sum_entry(struct f2fs_sb_info *sbi, int type,
<<<<<<< HEAD
					struct f2fs_summary *sum)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	void *addr = curseg->sum_blk;
	addr += curseg->next_blkoff * sizeof(struct f2fs_summary);
	memcpy(addr, sum, sizeof(struct f2fs_summary));
=======
		struct f2fs_summary *sum, unsigned short offset)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	void *addr = curseg->sum_blk;
	addr += offset * sizeof(struct f2fs_summary);
	memcpy(addr, sum, sizeof(struct f2fs_summary));
	return;
>>>>>>> 512ca3c... stock
}

/*
 * Calculate the number of current summary pages for writing
 */
<<<<<<< HEAD
int npages_for_summary_flush(struct f2fs_sb_info *sbi, bool for_ra)
{
	int valid_sum_count = 0;
	int i, sum_in_page;
=======
int npages_for_summary_flush(struct f2fs_sb_info *sbi)
{
	int total_size_bytes = 0;
	int valid_sum_count = 0;
	int i, sum_space;
>>>>>>> 512ca3c... stock

	for (i = CURSEG_HOT_DATA; i <= CURSEG_COLD_DATA; i++) {
		if (sbi->ckpt->alloc_type[i] == SSR)
			valid_sum_count += sbi->blocks_per_seg;
<<<<<<< HEAD
		else {
			if (for_ra)
				valid_sum_count += le16_to_cpu(
					F2FS_CKPT(sbi)->cur_data_blkoff[i]);
			else
				valid_sum_count += curseg_blkoff(sbi, i);
		}
	}

	sum_in_page = (PAGE_CACHE_SIZE - 2 * SUM_JOURNAL_SIZE -
			SUM_FOOTER_SIZE) / SUMMARY_SIZE;
	if (valid_sum_count <= sum_in_page)
		return 1;
	else if ((valid_sum_count - sum_in_page) <=
		(PAGE_CACHE_SIZE - SUM_FOOTER_SIZE) / SUMMARY_SIZE)
=======
		else
			valid_sum_count += curseg_blkoff(sbi, i);
	}

	total_size_bytes = valid_sum_count * (SUMMARY_SIZE + 1)
			+ sizeof(struct nat_journal) + 2
			+ sizeof(struct sit_journal) + 2;
	sum_space = PAGE_CACHE_SIZE - SUM_FOOTER_SIZE;
	if (total_size_bytes < sum_space)
		return 1;
	else if (total_size_bytes < 2 * sum_space)
>>>>>>> 512ca3c... stock
		return 2;
	return 3;
}

/*
 * Caller should put this summary page
 */
struct page *get_sum_page(struct f2fs_sb_info *sbi, unsigned int segno)
{
	return get_meta_page(sbi, GET_SUM_BLOCK(sbi, segno));
}

<<<<<<< HEAD
void update_meta_page(struct f2fs_sb_info *sbi, void *src, block_t blk_addr)
{
	struct page *page = grab_meta_page(sbi, blk_addr);
	void *dst = page_address(page);

	if (src)
		memcpy(dst, src, PAGE_CACHE_SIZE);
	else
		memset(dst, 0, PAGE_CACHE_SIZE);
=======
static void write_sum_page(struct f2fs_sb_info *sbi,
			struct f2fs_summary_block *sum_blk, block_t blk_addr)
{
	struct page *page = grab_meta_page(sbi, blk_addr);
	void *kaddr = page_address(page);
	memcpy(kaddr, sum_blk, PAGE_CACHE_SIZE);
>>>>>>> 512ca3c... stock
	set_page_dirty(page);
	f2fs_put_page(page, 1);
}

<<<<<<< HEAD
static void write_sum_page(struct f2fs_sb_info *sbi,
			struct f2fs_summary_block *sum_blk, block_t blk_addr)
{
	update_meta_page(sbi, (void *)sum_blk, blk_addr);
}

static void write_current_sum_page(struct f2fs_sb_info *sbi,
						int type, block_t blk_addr)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	struct page *page = grab_meta_page(sbi, blk_addr);
	struct f2fs_summary_block *src = curseg->sum_blk;
	struct f2fs_summary_block *dst;

	dst = (struct f2fs_summary_block *)page_address(page);

	mutex_lock(&curseg->curseg_mutex);

	down_read(&curseg->journal_rwsem);
	memcpy(&dst->journal, curseg->journal, SUM_JOURNAL_SIZE);
	up_read(&curseg->journal_rwsem);

	memcpy(dst->entries, src->entries, SUM_ENTRY_SIZE);
	memcpy(&dst->footer, &src->footer, SUM_FOOTER_SIZE);

	mutex_unlock(&curseg->curseg_mutex);

	set_page_dirty(page);
	f2fs_put_page(page, 1);
=======
static unsigned int check_prefree_segments(struct f2fs_sb_info *sbi, int type)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	unsigned long *prefree_segmap = dirty_i->dirty_segmap[PRE];
	unsigned int segno;
	unsigned int ofs = 0;

	/*
	 * If there is not enough reserved sections,
	 * we should not reuse prefree segments.
	 */
	if (has_not_enough_free_secs(sbi, 0))
		return NULL_SEGNO;

	/*
	 * NODE page should not reuse prefree segment,
	 * since those information is used for SPOR.
	 */
	if (IS_NODESEG(type))
		return NULL_SEGNO;
next:
	segno = find_next_bit(prefree_segmap, TOTAL_SEGS(sbi), ofs);
	ofs += sbi->segs_per_sec;

	if (segno < TOTAL_SEGS(sbi)) {
		int i;

		/* skip intermediate segments in a section */
		if (segno % sbi->segs_per_sec)
			goto next;

		/* skip if the section is currently used */
		if (sec_usage_check(sbi, GET_SECNO(sbi, segno)))
			goto next;

		/* skip if whole section is not prefree */
		for (i = 1; i < sbi->segs_per_sec; i++)
			if (!test_bit(segno + i, prefree_segmap))
				goto next;

		/* skip if whole section was not free at the last checkpoint */
		for (i = 0; i < sbi->segs_per_sec; i++)
			if (get_seg_entry(sbi, segno + i)->ckpt_valid_blocks)
				goto next;

		return segno;
	}
	return NULL_SEGNO;
>>>>>>> 512ca3c... stock
}

static int is_next_segment_free(struct f2fs_sb_info *sbi, int type)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
<<<<<<< HEAD
	unsigned int segno = curseg->segno + 1;
	struct free_segmap_info *free_i = FREE_I(sbi);

	if (segno < MAIN_SEGS(sbi) && segno % sbi->segs_per_sec)
		return !test_bit(segno, free_i->free_segmap);
=======
	unsigned int segno = curseg->segno;
	struct free_segmap_info *free_i = FREE_I(sbi);

	if (segno + 1 < TOTAL_SEGS(sbi) && (segno + 1) % sbi->segs_per_sec)
		return !test_bit(segno + 1, free_i->free_segmap);
>>>>>>> 512ca3c... stock
	return 0;
}

/*
 * Find a new segment from the free segments bitmap to right order
 * This function should be returned with success, otherwise BUG
 */
static void get_new_segment(struct f2fs_sb_info *sbi,
			unsigned int *newseg, bool new_sec, int dir)
{
	struct free_segmap_info *free_i = FREE_I(sbi);
	unsigned int segno, secno, zoneno;
<<<<<<< HEAD
	unsigned int total_zones = MAIN_SECS(sbi) / sbi->secs_per_zone;
=======
	unsigned int total_zones = TOTAL_SECS(sbi) / sbi->secs_per_zone;
>>>>>>> 512ca3c... stock
	unsigned int hint = *newseg / sbi->segs_per_sec;
	unsigned int old_zoneno = GET_ZONENO_FROM_SEGNO(sbi, *newseg);
	unsigned int left_start = hint;
	bool init = true;
	int go_left = 0;
	int i;

<<<<<<< HEAD
	spin_lock(&free_i->segmap_lock);

	if (!new_sec && ((*newseg + 1) % sbi->segs_per_sec)) {
		segno = find_next_zero_bit(free_i->free_segmap,
				(hint + 1) * sbi->segs_per_sec, *newseg + 1);
		if (segno < (hint + 1) * sbi->segs_per_sec)
			goto got_it;
	}
find_other_zone:
	secno = find_next_zero_bit(free_i->free_secmap, MAIN_SECS(sbi), hint);
	if (secno >= MAIN_SECS(sbi)) {
		if (dir == ALLOC_RIGHT) {
			secno = find_next_zero_bit(free_i->free_secmap,
							MAIN_SECS(sbi), 0);
			f2fs_bug_on(sbi, secno >= MAIN_SECS(sbi));
=======
	write_lock(&free_i->segmap_lock);

	if (!new_sec && ((*newseg + 1) % sbi->segs_per_sec)) {
		segno = find_next_zero_bit(free_i->free_segmap,
					TOTAL_SEGS(sbi), *newseg + 1);
		if (segno - *newseg < sbi->segs_per_sec -
					(*newseg % sbi->segs_per_sec))
			goto got_it;
	}
find_other_zone:
	secno = find_next_zero_bit(free_i->free_secmap, TOTAL_SECS(sbi), hint);
	if (secno >= TOTAL_SECS(sbi)) {
		if (dir == ALLOC_RIGHT) {
			secno = find_next_zero_bit(free_i->free_secmap,
							TOTAL_SECS(sbi), 0);
			BUG_ON(secno >= TOTAL_SECS(sbi));
>>>>>>> 512ca3c... stock
		} else {
			go_left = 1;
			left_start = hint - 1;
		}
	}
	if (go_left == 0)
		goto skip_left;

	while (test_bit(left_start, free_i->free_secmap)) {
		if (left_start > 0) {
			left_start--;
			continue;
		}
		left_start = find_next_zero_bit(free_i->free_secmap,
<<<<<<< HEAD
							MAIN_SECS(sbi), 0);
		f2fs_bug_on(sbi, left_start >= MAIN_SECS(sbi));
=======
							TOTAL_SECS(sbi), 0);
		BUG_ON(left_start >= TOTAL_SECS(sbi));
>>>>>>> 512ca3c... stock
		break;
	}
	secno = left_start;
skip_left:
	hint = secno;
	segno = secno * sbi->segs_per_sec;
	zoneno = secno / sbi->secs_per_zone;

	/* give up on finding another zone */
	if (!init)
		goto got_it;
	if (sbi->secs_per_zone == 1)
		goto got_it;
	if (zoneno == old_zoneno)
		goto got_it;
	if (dir == ALLOC_LEFT) {
		if (!go_left && zoneno + 1 >= total_zones)
			goto got_it;
		if (go_left && zoneno == 0)
			goto got_it;
	}
	for (i = 0; i < NR_CURSEG_TYPE; i++)
		if (CURSEG_I(sbi, i)->zone == zoneno)
			break;

	if (i < NR_CURSEG_TYPE) {
		/* zone is in user, try another */
		if (go_left)
			hint = zoneno * sbi->secs_per_zone - 1;
		else if (zoneno + 1 >= total_zones)
			hint = 0;
		else
			hint = (zoneno + 1) * sbi->secs_per_zone;
		init = false;
		goto find_other_zone;
	}
got_it:
	/* set it as dirty segment in free segmap */
<<<<<<< HEAD
	f2fs_bug_on(sbi, test_bit(segno, free_i->free_segmap));
	__set_inuse(sbi, segno);
	*newseg = segno;
	spin_unlock(&free_i->segmap_lock);
=======
	BUG_ON(test_bit(segno, free_i->free_segmap));
	__set_inuse(sbi, segno);
	*newseg = segno;
	write_unlock(&free_i->segmap_lock);
>>>>>>> 512ca3c... stock
}

static void reset_curseg(struct f2fs_sb_info *sbi, int type, int modified)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	struct summary_footer *sum_footer;

	curseg->segno = curseg->next_segno;
	curseg->zone = GET_ZONENO_FROM_SEGNO(sbi, curseg->segno);
	curseg->next_blkoff = 0;
	curseg->next_segno = NULL_SEGNO;

	sum_footer = &(curseg->sum_blk->footer);
	memset(sum_footer, 0, sizeof(struct summary_footer));
	if (IS_DATASEG(type))
		SET_SUM_TYPE(sum_footer, SUM_TYPE_DATA);
	if (IS_NODESEG(type))
		SET_SUM_TYPE(sum_footer, SUM_TYPE_NODE);
	__set_sit_entry_type(sbi, type, curseg->segno, modified);
}

/*
 * Allocate a current working segment.
 * This function always allocates a free segment in LFS manner.
 */
static void new_curseg(struct f2fs_sb_info *sbi, int type, bool new_sec)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	unsigned int segno = curseg->segno;
	int dir = ALLOC_LEFT;

	write_sum_page(sbi, curseg->sum_blk,
<<<<<<< HEAD
				GET_SUM_BLOCK(sbi, segno));
=======
				GET_SUM_BLOCK(sbi, curseg->segno));
>>>>>>> 512ca3c... stock
	if (type == CURSEG_WARM_DATA || type == CURSEG_COLD_DATA)
		dir = ALLOC_RIGHT;

	if (test_opt(sbi, NOHEAP))
		dir = ALLOC_RIGHT;

	get_new_segment(sbi, &segno, new_sec, dir);
	curseg->next_segno = segno;
	reset_curseg(sbi, type, 1);
	curseg->alloc_type = LFS;
}

static void __next_free_blkoff(struct f2fs_sb_info *sbi,
			struct curseg_info *seg, block_t start)
{
	struct seg_entry *se = get_seg_entry(sbi, seg->segno);
<<<<<<< HEAD
	int entries = SIT_VBLOCK_MAP_SIZE / sizeof(unsigned long);
	unsigned long *target_map = SIT_I(sbi)->tmp_map;
	unsigned long *ckpt_map = (unsigned long *)se->ckpt_valid_map;
	unsigned long *cur_map = (unsigned long *)se->cur_valid_map;
	int i, pos;

	for (i = 0; i < entries; i++)
		target_map[i] = ckpt_map[i] | cur_map[i];

	pos = __find_rev_next_zero_bit(target_map, sbi->blocks_per_seg, start);

	seg->next_blkoff = pos;
=======
	block_t ofs;
	for (ofs = start; ofs < sbi->blocks_per_seg; ofs++) {
		if (!f2fs_test_bit(ofs, se->ckpt_valid_map)
			&& !f2fs_test_bit(ofs, se->cur_valid_map))
			break;
	}
	seg->next_blkoff = ofs;
>>>>>>> 512ca3c... stock
}

/*
 * If a segment is written by LFS manner, next block offset is just obtained
 * by increasing the current block offset. However, if a segment is written by
 * SSR manner, next block offset obtained by calling __next_free_blkoff
 */
static void __refresh_next_blkoff(struct f2fs_sb_info *sbi,
				struct curseg_info *seg)
{
	if (seg->alloc_type == SSR)
		__next_free_blkoff(sbi, seg, seg->next_blkoff + 1);
	else
		seg->next_blkoff++;
}

/*
<<<<<<< HEAD
 * This function always allocates a used segment(from dirty seglist) by SSR
=======
 * This function always allocates a used segment (from dirty seglist) by SSR
>>>>>>> 512ca3c... stock
 * manner, so it should recover the existing segment information of valid blocks
 */
static void change_curseg(struct f2fs_sb_info *sbi, int type, bool reuse)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	unsigned int new_segno = curseg->next_segno;
	struct f2fs_summary_block *sum_node;
	struct page *sum_page;

	write_sum_page(sbi, curseg->sum_blk,
				GET_SUM_BLOCK(sbi, curseg->segno));
	__set_test_and_inuse(sbi, new_segno);

	mutex_lock(&dirty_i->seglist_lock);
	__remove_dirty_segment(sbi, new_segno, PRE);
	__remove_dirty_segment(sbi, new_segno, DIRTY);
	mutex_unlock(&dirty_i->seglist_lock);

	reset_curseg(sbi, type, 1);
	curseg->alloc_type = SSR;
	__next_free_blkoff(sbi, curseg, 0);

	if (reuse) {
		sum_page = get_sum_page(sbi, new_segno);
		sum_node = (struct f2fs_summary_block *)page_address(sum_page);
		memcpy(curseg->sum_blk, sum_node, SUM_ENTRY_SIZE);
		f2fs_put_page(sum_page, 1);
	}
}

static int get_ssr_segment(struct f2fs_sb_info *sbi, int type)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	const struct victim_selection *v_ops = DIRTY_I(sbi)->v_ops;

	if (IS_NODESEG(type) || !has_not_enough_free_secs(sbi, 0))
		return v_ops->get_victim(sbi,
				&(curseg)->next_segno, BG_GC, type, SSR);

	/* For data segments, let's do SSR more intensively */
	for (; type >= CURSEG_HOT_DATA; type--)
		if (v_ops->get_victim(sbi, &(curseg)->next_segno,
						BG_GC, type, SSR))
			return 1;
	return 0;
}

/*
 * flush out current segment and replace it with new segment
 * This function should be returned with success, otherwise BUG
 */
static void allocate_segment_by_default(struct f2fs_sb_info *sbi,
						int type, bool force)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);

<<<<<<< HEAD
	if (force)
		new_curseg(sbi, type, true);
=======
	if (force) {
		new_curseg(sbi, type, true);
		goto out;
	}

	curseg->next_segno = check_prefree_segments(sbi, type);

	if (curseg->next_segno != NULL_SEGNO)
		change_curseg(sbi, type, false);
>>>>>>> 512ca3c... stock
	else if (type == CURSEG_WARM_NODE)
		new_curseg(sbi, type, false);
	else if (curseg->alloc_type == LFS && is_next_segment_free(sbi, type))
		new_curseg(sbi, type, false);
	else if (need_SSR(sbi) && get_ssr_segment(sbi, type))
		change_curseg(sbi, type, true);
	else
		new_curseg(sbi, type, false);
<<<<<<< HEAD

	stat_inc_seg_type(sbi, curseg);
}

static void __allocate_new_segments(struct f2fs_sb_info *sbi, int type)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	unsigned int old_segno;

	old_segno = curseg->segno;
	SIT_I(sbi)->s_ops->allocate_segment(sbi, type, true);
	locate_dirty_segment(sbi, old_segno);
=======
out:
	sbi->segment_count[curseg->alloc_type]++;
>>>>>>> 512ca3c... stock
}

void allocate_new_segments(struct f2fs_sb_info *sbi)
{
<<<<<<< HEAD
	int i;

	for (i = CURSEG_HOT_DATA; i <= CURSEG_COLD_DATA; i++)
		__allocate_new_segments(sbi, i);
=======
	struct curseg_info *curseg;
	unsigned int old_curseg;
	int i;

	for (i = CURSEG_HOT_DATA; i <= CURSEG_COLD_DATA; i++) {
		curseg = CURSEG_I(sbi, i);
		old_curseg = curseg->segno;
		SIT_I(sbi)->s_ops->allocate_segment(sbi, i, true);
		locate_dirty_segment(sbi, old_curseg);
	}
>>>>>>> 512ca3c... stock
}

static const struct segment_allocation default_salloc_ops = {
	.allocate_segment = allocate_segment_by_default,
};

<<<<<<< HEAD
int f2fs_trim_fs(struct f2fs_sb_info *sbi, struct fstrim_range *range)
{
	__u64 start = F2FS_BYTES_TO_BLK(range->start);
	__u64 end = start + F2FS_BYTES_TO_BLK(range->len) - 1;
	unsigned int start_segno, end_segno;
	struct cp_control cpc;
	int err = 0;

	if (start >= MAX_BLKADDR(sbi) || range->len < sbi->blocksize)
		return -EINVAL;

	cpc.trimmed = 0;
	if (end <= MAIN_BLKADDR(sbi))
		goto out;

	/* start/end segment number in main_area */
	start_segno = (start <= MAIN_BLKADDR(sbi)) ? 0 : GET_SEGNO(sbi, start);
	end_segno = (end >= MAX_BLKADDR(sbi)) ? MAIN_SEGS(sbi) - 1 :
						GET_SEGNO(sbi, end);
	cpc.reason = CP_DISCARD;
	cpc.trim_minlen = max_t(__u64, 1, F2FS_BYTES_TO_BLK(range->minlen));

	/* do checkpoint to issue discard commands safely */
	for (; start_segno <= end_segno; start_segno = cpc.trim_end + 1) {
		cpc.trim_start = start_segno;

		if (sbi->discard_blks == 0)
			break;
		else if (sbi->discard_blks < BATCHED_TRIM_BLOCKS(sbi))
			cpc.trim_end = end_segno;
		else
			cpc.trim_end = min_t(unsigned int,
				rounddown(start_segno +
				BATCHED_TRIM_SEGMENTS(sbi),
				sbi->segs_per_sec) - 1, end_segno);

		mutex_lock(&sbi->gc_mutex);
		err = write_checkpoint(sbi, &cpc);
		mutex_unlock(&sbi->gc_mutex);
	}
out:
	range->len = F2FS_BLK_TO_BYTES(cpc.trimmed);
	return err;
=======
static void f2fs_end_io_write(struct bio *bio, int err)
{
	const int uptodate = test_bit(BIO_UPTODATE, &bio->bi_flags);
	struct bio_vec *bvec = bio->bi_io_vec + bio->bi_vcnt - 1;
	struct bio_private *p = bio->bi_private;

	do {
		struct page *page = bvec->bv_page;

		if (--bvec >= bio->bi_io_vec)
			prefetchw(&bvec->bv_page->flags);
		if (!uptodate) {
			SetPageError(page);
			if (page->mapping)
				set_bit(AS_EIO, &page->mapping->flags);
			set_ckpt_flags(p->sbi->ckpt, CP_ERROR_FLAG);
			p->sbi->sb->s_flags |= MS_RDONLY;
		}
		end_page_writeback(page);
		dec_page_count(p->sbi, F2FS_WRITEBACK);
	} while (bvec >= bio->bi_io_vec);

	if (p->is_sync)
		complete(p->wait);
	kfree(p);
	bio_put(bio);
}

struct bio *f2fs_bio_alloc(struct block_device *bdev, int npages)
{
	struct bio *bio;
	struct bio_private *priv;
retry:
	priv = kmalloc(sizeof(struct bio_private), GFP_NOFS);
	if (!priv) {
		cond_resched();
		goto retry;
	}

	/* No failure on bio allocation */
	bio = bio_alloc(GFP_NOIO, npages);
	bio->bi_bdev = bdev;
	bio->bi_private = priv;
	return bio;
}

static void do_submit_bio(struct f2fs_sb_info *sbi,
				enum page_type type, bool sync)
{
	int rw = sync ? WRITE_SYNC : WRITE;
	enum page_type btype = type > META ? META : type;

	if (type >= META_FLUSH)
		rw = WRITE_FLUSH_FUA;

	if (btype == META)
		rw |= REQ_META;

	if (sbi->bio[btype]) {
		struct bio_private *p = sbi->bio[btype]->bi_private;
		p->sbi = sbi;
		sbi->bio[btype]->bi_end_io = f2fs_end_io_write;

		trace_f2fs_do_submit_bio(sbi->sb, btype, sync, sbi->bio[btype]);

		if (type == META_FLUSH) {
			DECLARE_COMPLETION_ONSTACK(wait);
			p->is_sync = true;
			p->wait = &wait;
			submit_bio(rw, sbi->bio[btype]);
			wait_for_completion(&wait);
		} else {
			p->is_sync = false;
			submit_bio(rw, sbi->bio[btype]);
		}
		sbi->bio[btype] = NULL;
	}
}

void f2fs_submit_bio(struct f2fs_sb_info *sbi, enum page_type type, bool sync)
{
	down_write(&sbi->bio_sem);
	do_submit_bio(sbi, type, sync);
	up_write(&sbi->bio_sem);
}

static void submit_write_page(struct f2fs_sb_info *sbi, struct page *page,
				block_t blk_addr, enum page_type type)
{
	struct block_device *bdev = sbi->sb->s_bdev;

	verify_block_addr(sbi, blk_addr);

	down_write(&sbi->bio_sem);

	inc_page_count(sbi, F2FS_WRITEBACK);

	if (sbi->bio[type] && sbi->last_block_in_bio[type] != blk_addr - 1)
		do_submit_bio(sbi, type, false);
alloc_new:
	if (sbi->bio[type] == NULL) {
		sbi->bio[type] = f2fs_bio_alloc(bdev, max_hw_blocks(sbi));
		sbi->bio[type]->bi_sector = SECTOR_FROM_BLOCK(sbi, blk_addr);
		/*
		 * The end_io will be assigned at the sumbission phase.
		 * Until then, let bio_add_page() merge consecutive IOs as much
		 * as possible.
		 */
	}

	if (bio_add_page(sbi->bio[type], page, PAGE_CACHE_SIZE, 0) <
							PAGE_CACHE_SIZE) {
		do_submit_bio(sbi, type, false);
		goto alloc_new;
	}

	sbi->last_block_in_bio[type] = blk_addr;

	up_write(&sbi->bio_sem);
	trace_f2fs_submit_write_page(page, blk_addr, type);
>>>>>>> 512ca3c... stock
}

static bool __has_curseg_space(struct f2fs_sb_info *sbi, int type)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);
	if (curseg->next_blkoff < sbi->blocks_per_seg)
		return true;
	return false;
}

static int __get_segment_type_2(struct page *page, enum page_type p_type)
{
	if (p_type == DATA)
		return CURSEG_HOT_DATA;
	else
		return CURSEG_HOT_NODE;
}

static int __get_segment_type_4(struct page *page, enum page_type p_type)
{
	if (p_type == DATA) {
		struct inode *inode = page->mapping->host;

		if (S_ISDIR(inode->i_mode))
			return CURSEG_HOT_DATA;
		else
			return CURSEG_COLD_DATA;
	} else {
<<<<<<< HEAD
		if (IS_DNODE(page) && is_cold_node(page))
			return CURSEG_WARM_NODE;
=======
		if (IS_DNODE(page) && !is_cold_node(page))
			return CURSEG_HOT_NODE;
>>>>>>> 512ca3c... stock
		else
			return CURSEG_COLD_NODE;
	}
}

static int __get_segment_type_6(struct page *page, enum page_type p_type)
{
	if (p_type == DATA) {
		struct inode *inode = page->mapping->host;

		if (S_ISDIR(inode->i_mode))
			return CURSEG_HOT_DATA;
<<<<<<< HEAD
		else if (is_cold_data(page) || file_is_cold(inode))
=======
		else if (is_cold_data(page) || is_cold_file(inode))
>>>>>>> 512ca3c... stock
			return CURSEG_COLD_DATA;
		else
			return CURSEG_WARM_DATA;
	} else {
		if (IS_DNODE(page))
			return is_cold_node(page) ? CURSEG_WARM_NODE :
						CURSEG_HOT_NODE;
		else
			return CURSEG_COLD_NODE;
	}
}

static int __get_segment_type(struct page *page, enum page_type p_type)
{
<<<<<<< HEAD
	switch (F2FS_P_SB(page)->active_logs) {
=======
	struct f2fs_sb_info *sbi = F2FS_SB(page->mapping->host->i_sb);
	switch (sbi->active_logs) {
>>>>>>> 512ca3c... stock
	case 2:
		return __get_segment_type_2(page, p_type);
	case 4:
		return __get_segment_type_4(page, p_type);
	}
	/* NR_CURSEG_TYPE(6) logs by default */
<<<<<<< HEAD
	f2fs_bug_on(F2FS_P_SB(page),
		F2FS_P_SB(page)->active_logs != NR_CURSEG_TYPE);
	return __get_segment_type_6(page, p_type);
}

void allocate_data_block(struct f2fs_sb_info *sbi, struct page *page,
		block_t old_blkaddr, block_t *new_blkaddr,
		struct f2fs_summary *sum, int type)
{
	struct sit_info *sit_i = SIT_I(sbi);
	struct curseg_info *curseg;
	bool direct_io = (type == CURSEG_DIRECT_IO);

	type = direct_io ? CURSEG_WARM_DATA : type;

	curseg = CURSEG_I(sbi, type);

	mutex_lock(&curseg->curseg_mutex);
	mutex_lock(&sit_i->sentry_lock);

	/* direct_io'ed data is aligned to the segment for better performance */
	if (direct_io && curseg->next_blkoff &&
				!has_not_enough_free_secs(sbi, 0))
		__allocate_new_segments(sbi, type);

	*new_blkaddr = NEXT_FREE_BLKADDR(sbi, curseg);
=======
	BUG_ON(sbi->active_logs != NR_CURSEG_TYPE);
	return __get_segment_type_6(page, p_type);
}

static void do_write_page(struct f2fs_sb_info *sbi, struct page *page,
			block_t old_blkaddr, block_t *new_blkaddr,
			struct f2fs_summary *sum, enum page_type p_type)
{
	struct sit_info *sit_i = SIT_I(sbi);
	struct curseg_info *curseg;
	unsigned int old_cursegno;
	int type;

	type = __get_segment_type(page, p_type);
	curseg = CURSEG_I(sbi, type);

	mutex_lock(&curseg->curseg_mutex);

	*new_blkaddr = NEXT_FREE_BLKADDR(sbi, curseg);
	old_cursegno = curseg->segno;
>>>>>>> 512ca3c... stock

	/*
	 * __add_sum_entry should be resided under the curseg_mutex
	 * because, this function updates a summary entry in the
	 * current summary block.
	 */
<<<<<<< HEAD
	__add_sum_entry(sbi, type, sum);

	__refresh_next_blkoff(sbi, curseg);

	stat_inc_block_count(sbi, curseg);

	if (!__has_curseg_space(sbi, type))
		sit_i->s_ops->allocate_segment(sbi, type, false);
=======
	__add_sum_entry(sbi, type, sum, curseg->next_blkoff);

	mutex_lock(&sit_i->sentry_lock);
	__refresh_next_blkoff(sbi, curseg);
	sbi->block_count[curseg->alloc_type]++;

>>>>>>> 512ca3c... stock
	/*
	 * SIT information should be updated before segment allocation,
	 * since SSR needs latest valid block information.
	 */
	refresh_sit_entry(sbi, old_blkaddr, *new_blkaddr);

<<<<<<< HEAD
	mutex_unlock(&sit_i->sentry_lock);

	if (page && IS_NODESEG(type))
		fill_node_footer_blkaddr(page, NEXT_FREE_BLKADDR(sbi, curseg));

	mutex_unlock(&curseg->curseg_mutex);
}

static void do_write_page(struct f2fs_summary *sum, struct f2fs_io_info *fio)
{
	int type = __get_segment_type(fio->page, fio->type);

	allocate_data_block(fio->sbi, fio->page, fio->old_blkaddr,
					&fio->new_blkaddr, sum, type);

	/* writeout dirty page into bdev */
	f2fs_submit_page_mbio(fio);
=======
	if (!__has_curseg_space(sbi, type))
		sit_i->s_ops->allocate_segment(sbi, type, false);

	locate_dirty_segment(sbi, old_cursegno);
	locate_dirty_segment(sbi, GET_SEGNO(sbi, old_blkaddr));
	mutex_unlock(&sit_i->sentry_lock);

	if (p_type == NODE)
		fill_node_footer_blkaddr(page, NEXT_FREE_BLKADDR(sbi, curseg));

	/* writeout dirty page into bdev */
	submit_write_page(sbi, page, *new_blkaddr, p_type);

	mutex_unlock(&curseg->curseg_mutex);
>>>>>>> 512ca3c... stock
}

void write_meta_page(struct f2fs_sb_info *sbi, struct page *page)
{
<<<<<<< HEAD
	struct f2fs_io_info fio = {
		.sbi = sbi,
		.type = META,
		.rw = WRITE_SYNC | REQ_META | REQ_PRIO,
		.old_blkaddr = page->index,
		.new_blkaddr = page->index,
		.page = page,
		.encrypted_page = NULL,
	};

	if (unlikely(page->index >= MAIN_BLKADDR(sbi)))
		fio.rw &= ~REQ_META;

	set_page_writeback(page);
	f2fs_submit_page_mbio(&fio);
}

void write_node_page(unsigned int nid, struct f2fs_io_info *fio)
{
	struct f2fs_summary sum;

	set_summary(&sum, nid, 0, 0);
	do_write_page(&sum, fio);
}

void write_data_page(struct dnode_of_data *dn, struct f2fs_io_info *fio)
{
	struct f2fs_sb_info *sbi = fio->sbi;
	struct f2fs_summary sum;
	struct node_info ni;

	f2fs_bug_on(sbi, dn->data_blkaddr == NULL_ADDR);
	get_node_info(sbi, dn->nid, &ni);
	set_summary(&sum, dn->nid, dn->ofs_in_node, ni.version);
	do_write_page(&sum, fio);
	f2fs_update_data_blkaddr(dn, fio->new_blkaddr);
}

void rewrite_data_page(struct f2fs_io_info *fio)
{
	fio->new_blkaddr = fio->old_blkaddr;
	stat_inc_inplace_blocks(fio->sbi);
	f2fs_submit_page_mbio(fio);
}

void __f2fs_replace_block(struct f2fs_sb_info *sbi, struct f2fs_summary *sum,
				block_t old_blkaddr, block_t new_blkaddr,
				bool recover_curseg, bool recover_newaddr)
=======
	set_page_writeback(page);
	submit_write_page(sbi, page, page->index, META);
}

void write_node_page(struct f2fs_sb_info *sbi, struct page *page,
		unsigned int nid, block_t old_blkaddr, block_t *new_blkaddr)
{
	struct f2fs_summary sum;
	set_summary(&sum, nid, 0, 0);
	do_write_page(sbi, page, old_blkaddr, new_blkaddr, &sum, NODE);
}

void write_data_page(struct inode *inode, struct page *page,
		struct dnode_of_data *dn, block_t old_blkaddr,
		block_t *new_blkaddr)
{
	struct f2fs_sb_info *sbi = F2FS_SB(inode->i_sb);
	struct f2fs_summary sum;
	struct node_info ni;

	BUG_ON(old_blkaddr == NULL_ADDR);
	get_node_info(sbi, dn->nid, &ni);
	set_summary(&sum, dn->nid, dn->ofs_in_node, ni.version);

	do_write_page(sbi, page, old_blkaddr,
			new_blkaddr, &sum, DATA);
}

void rewrite_data_page(struct f2fs_sb_info *sbi, struct page *page,
					block_t old_blk_addr)
{
	submit_write_page(sbi, page, old_blk_addr, DATA);
}

void recover_data_page(struct f2fs_sb_info *sbi,
			struct page *page, struct f2fs_summary *sum,
			block_t old_blkaddr, block_t new_blkaddr)
>>>>>>> 512ca3c... stock
{
	struct sit_info *sit_i = SIT_I(sbi);
	struct curseg_info *curseg;
	unsigned int segno, old_cursegno;
	struct seg_entry *se;
	int type;
<<<<<<< HEAD
	unsigned short old_blkoff;
=======
>>>>>>> 512ca3c... stock

	segno = GET_SEGNO(sbi, new_blkaddr);
	se = get_seg_entry(sbi, segno);
	type = se->type;

<<<<<<< HEAD
	if (!recover_curseg) {
		/* for recovery flow */
		if (se->valid_blocks == 0 && !IS_CURSEG(sbi, segno)) {
			if (old_blkaddr == NULL_ADDR)
				type = CURSEG_COLD_DATA;
			else
				type = CURSEG_WARM_DATA;
		}
	} else {
		if (!IS_CURSEG(sbi, segno))
			type = CURSEG_WARM_DATA;
	}

=======
	if (se->valid_blocks == 0 && !IS_CURSEG(sbi, segno)) {
		if (old_blkaddr == NULL_ADDR)
			type = CURSEG_COLD_DATA;
		else
			type = CURSEG_WARM_DATA;
	}
>>>>>>> 512ca3c... stock
	curseg = CURSEG_I(sbi, type);

	mutex_lock(&curseg->curseg_mutex);
	mutex_lock(&sit_i->sentry_lock);

	old_cursegno = curseg->segno;
<<<<<<< HEAD
	old_blkoff = curseg->next_blkoff;
=======
>>>>>>> 512ca3c... stock

	/* change the current segment */
	if (segno != curseg->segno) {
		curseg->next_segno = segno;
		change_curseg(sbi, type, true);
	}

<<<<<<< HEAD
	curseg->next_blkoff = GET_BLKOFF_FROM_SEG0(sbi, new_blkaddr);
	__add_sum_entry(sbi, type, sum);

	if (!recover_curseg || recover_newaddr)
		update_sit_entry(sbi, new_blkaddr, 1);
	if (GET_SEGNO(sbi, old_blkaddr) != NULL_SEGNO)
		update_sit_entry(sbi, old_blkaddr, -1);

	locate_dirty_segment(sbi, GET_SEGNO(sbi, old_blkaddr));
	locate_dirty_segment(sbi, GET_SEGNO(sbi, new_blkaddr));

	locate_dirty_segment(sbi, old_cursegno);

	if (recover_curseg) {
		if (old_cursegno != curseg->segno) {
			curseg->next_segno = old_cursegno;
			change_curseg(sbi, type, true);
		}
		curseg->next_blkoff = old_blkoff;
	}
=======
	curseg->next_blkoff = GET_SEGOFF_FROM_SEG0(sbi, new_blkaddr) &
					(sbi->blocks_per_seg - 1);
	__add_sum_entry(sbi, type, sum, curseg->next_blkoff);

	refresh_sit_entry(sbi, old_blkaddr, new_blkaddr);

	locate_dirty_segment(sbi, old_cursegno);
	locate_dirty_segment(sbi, GET_SEGNO(sbi, old_blkaddr));
>>>>>>> 512ca3c... stock

	mutex_unlock(&sit_i->sentry_lock);
	mutex_unlock(&curseg->curseg_mutex);
}

<<<<<<< HEAD
void f2fs_replace_block(struct f2fs_sb_info *sbi, struct dnode_of_data *dn,
				block_t old_addr, block_t new_addr,
				unsigned char version, bool recover_curseg,
				bool recover_newaddr)
{
	struct f2fs_summary sum;

	set_summary(&sum, dn->nid, dn->ofs_in_node, version);

	__f2fs_replace_block(sbi, &sum, old_addr, new_addr,
					recover_curseg, recover_newaddr);

	f2fs_update_data_blkaddr(dn, new_addr);
}

void f2fs_wait_on_page_writeback(struct page *page,
				enum page_type type, bool ordered)
{
	if (PageWriteback(page)) {
		struct f2fs_sb_info *sbi = F2FS_P_SB(page);

		f2fs_submit_merged_bio_cond(sbi, NULL, page, 0, type, WRITE);
		if (ordered)
			wait_on_page_writeback(page);
		else
			wait_for_stable_page(page);
	}
}

void f2fs_wait_on_encrypted_page_writeback(struct f2fs_sb_info *sbi,
							block_t blkaddr)
{
	struct page *cpage;

	if (blkaddr == NEW_ADDR)
		return;

	f2fs_bug_on(sbi, blkaddr == NULL_ADDR);

	cpage = find_lock_page(META_MAPPING(sbi), blkaddr);
	if (cpage) {
		f2fs_wait_on_page_writeback(cpage, DATA, true);
		f2fs_put_page(cpage, 1);
	}
=======
void rewrite_node_page(struct f2fs_sb_info *sbi,
			struct page *page, struct f2fs_summary *sum,
			block_t old_blkaddr, block_t new_blkaddr)
{
	struct sit_info *sit_i = SIT_I(sbi);
	int type = CURSEG_WARM_NODE;
	struct curseg_info *curseg;
	unsigned int segno, old_cursegno;
	block_t next_blkaddr = next_blkaddr_of_node(page);
	unsigned int next_segno = GET_SEGNO(sbi, next_blkaddr);

	curseg = CURSEG_I(sbi, type);

	mutex_lock(&curseg->curseg_mutex);
	mutex_lock(&sit_i->sentry_lock);

	segno = GET_SEGNO(sbi, new_blkaddr);
	old_cursegno = curseg->segno;

	/* change the current segment */
	if (segno != curseg->segno) {
		curseg->next_segno = segno;
		change_curseg(sbi, type, true);
	}
	curseg->next_blkoff = GET_SEGOFF_FROM_SEG0(sbi, new_blkaddr) &
					(sbi->blocks_per_seg - 1);
	__add_sum_entry(sbi, type, sum, curseg->next_blkoff);

	/* change the current log to the next block addr in advance */
	if (next_segno != segno) {
		curseg->next_segno = next_segno;
		change_curseg(sbi, type, true);
	}
	curseg->next_blkoff = GET_SEGOFF_FROM_SEG0(sbi, next_blkaddr) &
					(sbi->blocks_per_seg - 1);

	/* rewrite node page */
	set_page_writeback(page);
	submit_write_page(sbi, page, new_blkaddr, NODE);
	f2fs_submit_bio(sbi, NODE, true);
	refresh_sit_entry(sbi, old_blkaddr, new_blkaddr);

	locate_dirty_segment(sbi, old_cursegno);
	locate_dirty_segment(sbi, GET_SEGNO(sbi, old_blkaddr));

	mutex_unlock(&sit_i->sentry_lock);
	mutex_unlock(&curseg->curseg_mutex);
>>>>>>> 512ca3c... stock
}

static int read_compacted_summaries(struct f2fs_sb_info *sbi)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct curseg_info *seg_i;
	unsigned char *kaddr;
	struct page *page;
	block_t start;
	int i, j, offset;

	start = start_sum_block(sbi);

	page = get_meta_page(sbi, start++);
	kaddr = (unsigned char *)page_address(page);

	/* Step 1: restore nat cache */
	seg_i = CURSEG_I(sbi, CURSEG_HOT_DATA);
<<<<<<< HEAD
	memcpy(seg_i->journal, kaddr, SUM_JOURNAL_SIZE);

	/* Step 2: restore sit cache */
	seg_i = CURSEG_I(sbi, CURSEG_COLD_DATA);
	memcpy(seg_i->journal, kaddr + SUM_JOURNAL_SIZE, SUM_JOURNAL_SIZE);
=======
	memcpy(&seg_i->sum_blk->n_nats, kaddr, SUM_JOURNAL_SIZE);

	/* Step 2: restore sit cache */
	seg_i = CURSEG_I(sbi, CURSEG_COLD_DATA);
	memcpy(&seg_i->sum_blk->n_sits, kaddr + SUM_JOURNAL_SIZE,
						SUM_JOURNAL_SIZE);
>>>>>>> 512ca3c... stock
	offset = 2 * SUM_JOURNAL_SIZE;

	/* Step 3: restore summary entries */
	for (i = CURSEG_HOT_DATA; i <= CURSEG_COLD_DATA; i++) {
		unsigned short blk_off;
		unsigned int segno;

		seg_i = CURSEG_I(sbi, i);
		segno = le32_to_cpu(ckpt->cur_data_segno[i]);
		blk_off = le16_to_cpu(ckpt->cur_data_blkoff[i]);
		seg_i->next_segno = segno;
		reset_curseg(sbi, i, 0);
		seg_i->alloc_type = ckpt->alloc_type[i];
		seg_i->next_blkoff = blk_off;

		if (seg_i->alloc_type == SSR)
			blk_off = sbi->blocks_per_seg;

		for (j = 0; j < blk_off; j++) {
			struct f2fs_summary *s;
			s = (struct f2fs_summary *)(kaddr + offset);
			seg_i->sum_blk->entries[j] = *s;
			offset += SUMMARY_SIZE;
			if (offset + SUMMARY_SIZE <= PAGE_CACHE_SIZE -
						SUM_FOOTER_SIZE)
				continue;

			f2fs_put_page(page, 1);
			page = NULL;

			page = get_meta_page(sbi, start++);
			kaddr = (unsigned char *)page_address(page);
			offset = 0;
		}
	}
	f2fs_put_page(page, 1);
	return 0;
}

static int read_normal_summaries(struct f2fs_sb_info *sbi, int type)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct f2fs_summary_block *sum;
	struct curseg_info *curseg;
	struct page *new;
	unsigned short blk_off;
	unsigned int segno = 0;
	block_t blk_addr = 0;

	/* get segment number and block addr */
	if (IS_DATASEG(type)) {
		segno = le32_to_cpu(ckpt->cur_data_segno[type]);
		blk_off = le16_to_cpu(ckpt->cur_data_blkoff[type -
							CURSEG_HOT_DATA]);
<<<<<<< HEAD
		if (__exist_node_summaries(sbi))
=======
		if (is_set_ckpt_flags(ckpt, CP_UMOUNT_FLAG))
>>>>>>> 512ca3c... stock
			blk_addr = sum_blk_addr(sbi, NR_CURSEG_TYPE, type);
		else
			blk_addr = sum_blk_addr(sbi, NR_CURSEG_DATA_TYPE, type);
	} else {
		segno = le32_to_cpu(ckpt->cur_node_segno[type -
							CURSEG_HOT_NODE]);
		blk_off = le16_to_cpu(ckpt->cur_node_blkoff[type -
							CURSEG_HOT_NODE]);
<<<<<<< HEAD
		if (__exist_node_summaries(sbi))
=======
		if (is_set_ckpt_flags(ckpt, CP_UMOUNT_FLAG))
>>>>>>> 512ca3c... stock
			blk_addr = sum_blk_addr(sbi, NR_CURSEG_NODE_TYPE,
							type - CURSEG_HOT_NODE);
		else
			blk_addr = GET_SUM_BLOCK(sbi, segno);
	}

	new = get_meta_page(sbi, blk_addr);
	sum = (struct f2fs_summary_block *)page_address(new);

	if (IS_NODESEG(type)) {
<<<<<<< HEAD
		if (__exist_node_summaries(sbi)) {
=======
		if (is_set_ckpt_flags(ckpt, CP_UMOUNT_FLAG)) {
>>>>>>> 512ca3c... stock
			struct f2fs_summary *ns = &sum->entries[0];
			int i;
			for (i = 0; i < sbi->blocks_per_seg; i++, ns++) {
				ns->version = 0;
				ns->ofs_in_node = 0;
			}
		} else {
<<<<<<< HEAD
			int err;

			err = restore_node_summary(sbi, segno, sum);
			if (err) {
				f2fs_put_page(new, 1);
				return err;
=======
			if (restore_node_summary(sbi, segno, sum)) {
				f2fs_put_page(new, 1);
				return -EINVAL;
>>>>>>> 512ca3c... stock
			}
		}
	}

	/* set uncompleted segment to curseg */
	curseg = CURSEG_I(sbi, type);
	mutex_lock(&curseg->curseg_mutex);
<<<<<<< HEAD

	/* update journal info */
	down_write(&curseg->journal_rwsem);
	memcpy(curseg->journal, &sum->journal, SUM_JOURNAL_SIZE);
	up_write(&curseg->journal_rwsem);

	memcpy(curseg->sum_blk->entries, sum->entries, SUM_ENTRY_SIZE);
	memcpy(&curseg->sum_blk->footer, &sum->footer, SUM_FOOTER_SIZE);
=======
	memcpy(curseg->sum_blk, sum, PAGE_CACHE_SIZE);
>>>>>>> 512ca3c... stock
	curseg->next_segno = segno;
	reset_curseg(sbi, type, 0);
	curseg->alloc_type = ckpt->alloc_type[type];
	curseg->next_blkoff = blk_off;
	mutex_unlock(&curseg->curseg_mutex);
	f2fs_put_page(new, 1);
	return 0;
}

static int restore_curseg_summaries(struct f2fs_sb_info *sbi)
{
	int type = CURSEG_HOT_DATA;
<<<<<<< HEAD
	int err;

	if (is_set_ckpt_flags(F2FS_CKPT(sbi), CP_COMPACT_SUM_FLAG)) {
		int npages = npages_for_summary_flush(sbi, true);

		if (npages >= 2)
			ra_meta_pages(sbi, start_sum_block(sbi), npages,
							META_CP, true);

=======

	if (is_set_ckpt_flags(F2FS_CKPT(sbi), CP_COMPACT_SUM_FLAG)) {
>>>>>>> 512ca3c... stock
		/* restore for compacted data summary */
		if (read_compacted_summaries(sbi))
			return -EINVAL;
		type = CURSEG_HOT_NODE;
	}

<<<<<<< HEAD
	if (__exist_node_summaries(sbi))
		ra_meta_pages(sbi, sum_blk_addr(sbi, NR_CURSEG_TYPE, type),
					NR_CURSEG_TYPE - type, META_CP, true);

	for (; type <= CURSEG_COLD_NODE; type++) {
		err = read_normal_summaries(sbi, type);
		if (err)
			return err;
	}

=======
	for (; type <= CURSEG_COLD_NODE; type++)
		if (read_normal_summaries(sbi, type))
			return -EINVAL;
>>>>>>> 512ca3c... stock
	return 0;
}

static void write_compacted_summaries(struct f2fs_sb_info *sbi, block_t blkaddr)
{
	struct page *page;
	unsigned char *kaddr;
	struct f2fs_summary *summary;
	struct curseg_info *seg_i;
	int written_size = 0;
	int i, j;

	page = grab_meta_page(sbi, blkaddr++);
	kaddr = (unsigned char *)page_address(page);

	/* Step 1: write nat cache */
	seg_i = CURSEG_I(sbi, CURSEG_HOT_DATA);
<<<<<<< HEAD
	memcpy(kaddr, seg_i->journal, SUM_JOURNAL_SIZE);
=======
	memcpy(kaddr, &seg_i->sum_blk->n_nats, SUM_JOURNAL_SIZE);
>>>>>>> 512ca3c... stock
	written_size += SUM_JOURNAL_SIZE;

	/* Step 2: write sit cache */
	seg_i = CURSEG_I(sbi, CURSEG_COLD_DATA);
<<<<<<< HEAD
	memcpy(kaddr + written_size, seg_i->journal, SUM_JOURNAL_SIZE);
	written_size += SUM_JOURNAL_SIZE;

=======
	memcpy(kaddr + written_size, &seg_i->sum_blk->n_sits,
						SUM_JOURNAL_SIZE);
	written_size += SUM_JOURNAL_SIZE;

	set_page_dirty(page);

>>>>>>> 512ca3c... stock
	/* Step 3: write summary entries */
	for (i = CURSEG_HOT_DATA; i <= CURSEG_COLD_DATA; i++) {
		unsigned short blkoff;
		seg_i = CURSEG_I(sbi, i);
		if (sbi->ckpt->alloc_type[i] == SSR)
			blkoff = sbi->blocks_per_seg;
		else
			blkoff = curseg_blkoff(sbi, i);

		for (j = 0; j < blkoff; j++) {
			if (!page) {
				page = grab_meta_page(sbi, blkaddr++);
				kaddr = (unsigned char *)page_address(page);
				written_size = 0;
			}
			summary = (struct f2fs_summary *)(kaddr + written_size);
			*summary = seg_i->sum_blk->entries[j];
			written_size += SUMMARY_SIZE;
<<<<<<< HEAD
=======
			set_page_dirty(page);
>>>>>>> 512ca3c... stock

			if (written_size + SUMMARY_SIZE <= PAGE_CACHE_SIZE -
							SUM_FOOTER_SIZE)
				continue;

<<<<<<< HEAD
			set_page_dirty(page);
=======
>>>>>>> 512ca3c... stock
			f2fs_put_page(page, 1);
			page = NULL;
		}
	}
<<<<<<< HEAD
	if (page) {
		set_page_dirty(page);
		f2fs_put_page(page, 1);
	}
=======
	if (page)
		f2fs_put_page(page, 1);
>>>>>>> 512ca3c... stock
}

static void write_normal_summaries(struct f2fs_sb_info *sbi,
					block_t blkaddr, int type)
{
	int i, end;
	if (IS_DATASEG(type))
		end = type + NR_CURSEG_DATA_TYPE;
	else
		end = type + NR_CURSEG_NODE_TYPE;

<<<<<<< HEAD
	for (i = type; i < end; i++)
		write_current_sum_page(sbi, i, blkaddr + (i - type));
=======
	for (i = type; i < end; i++) {
		struct curseg_info *sum = CURSEG_I(sbi, i);
		mutex_lock(&sum->curseg_mutex);
		write_sum_page(sbi, sum->sum_blk, blkaddr + (i - type));
		mutex_unlock(&sum->curseg_mutex);
	}
>>>>>>> 512ca3c... stock
}

void write_data_summaries(struct f2fs_sb_info *sbi, block_t start_blk)
{
	if (is_set_ckpt_flags(F2FS_CKPT(sbi), CP_COMPACT_SUM_FLAG))
		write_compacted_summaries(sbi, start_blk);
	else
		write_normal_summaries(sbi, start_blk, CURSEG_HOT_DATA);
}

void write_node_summaries(struct f2fs_sb_info *sbi, block_t start_blk)
{
<<<<<<< HEAD
	write_normal_summaries(sbi, start_blk, CURSEG_HOT_NODE);
}

int lookup_journal_in_cursum(struct f2fs_journal *journal, int type,
=======
	if (is_set_ckpt_flags(F2FS_CKPT(sbi), CP_UMOUNT_FLAG))
		write_normal_summaries(sbi, start_blk, CURSEG_HOT_NODE);
	return;
}

int lookup_journal_in_cursum(struct f2fs_summary_block *sum, int type,
>>>>>>> 512ca3c... stock
					unsigned int val, int alloc)
{
	int i;

	if (type == NAT_JOURNAL) {
<<<<<<< HEAD
		for (i = 0; i < nats_in_cursum(journal); i++) {
			if (le32_to_cpu(nid_in_journal(journal, i)) == val)
				return i;
		}
		if (alloc && __has_cursum_space(journal, 1, NAT_JOURNAL))
			return update_nats_in_cursum(journal, 1);
	} else if (type == SIT_JOURNAL) {
		for (i = 0; i < sits_in_cursum(journal); i++)
			if (le32_to_cpu(segno_in_journal(journal, i)) == val)
				return i;
		if (alloc && __has_cursum_space(journal, 1, SIT_JOURNAL))
			return update_sits_in_cursum(journal, 1);
=======
		for (i = 0; i < nats_in_cursum(sum); i++) {
			if (le32_to_cpu(nid_in_journal(sum, i)) == val)
				return i;
		}
		if (alloc && nats_in_cursum(sum) < NAT_JOURNAL_ENTRIES)
			return update_nats_in_cursum(sum, 1);
	} else if (type == SIT_JOURNAL) {
		for (i = 0; i < sits_in_cursum(sum); i++)
			if (le32_to_cpu(segno_in_journal(sum, i)) == val)
				return i;
		if (alloc && sits_in_cursum(sum) < SIT_JOURNAL_ENTRIES)
			return update_sits_in_cursum(sum, 1);
>>>>>>> 512ca3c... stock
	}
	return -1;
}

static struct page *get_current_sit_page(struct f2fs_sb_info *sbi,
					unsigned int segno)
{
<<<<<<< HEAD
	return get_meta_page(sbi, current_sit_addr(sbi, segno));
=======
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned int offset = SIT_BLOCK_OFFSET(sit_i, segno);
	block_t blk_addr = sit_i->sit_base_addr + offset;

	check_seg_range(sbi, segno);

	/* calculate sit block address */
	if (f2fs_test_bit(offset, sit_i->sit_bitmap))
		blk_addr += sit_i->sit_blocks;

	return get_meta_page(sbi, blk_addr);
>>>>>>> 512ca3c... stock
}

static struct page *get_next_sit_page(struct f2fs_sb_info *sbi,
					unsigned int start)
{
	struct sit_info *sit_i = SIT_I(sbi);
	struct page *src_page, *dst_page;
	pgoff_t src_off, dst_off;
	void *src_addr, *dst_addr;

	src_off = current_sit_addr(sbi, start);
	dst_off = next_sit_addr(sbi, src_off);

	/* get current sit block page without lock */
	src_page = get_meta_page(sbi, src_off);
	dst_page = grab_meta_page(sbi, dst_off);
<<<<<<< HEAD
	f2fs_bug_on(sbi, PageDirty(src_page));
=======
	BUG_ON(PageDirty(src_page));
>>>>>>> 512ca3c... stock

	src_addr = page_address(src_page);
	dst_addr = page_address(dst_page);
	memcpy(dst_addr, src_addr, PAGE_CACHE_SIZE);

	set_page_dirty(dst_page);
	f2fs_put_page(src_page, 1);

	set_to_next_sit(sit_i, start);

	return dst_page;
}

<<<<<<< HEAD
static struct sit_entry_set *grab_sit_entry_set(void)
{
	struct sit_entry_set *ses =
			f2fs_kmem_cache_alloc(sit_entry_set_slab, GFP_NOFS);

	ses->entry_cnt = 0;
	INIT_LIST_HEAD(&ses->set_list);
	return ses;
}

static void release_sit_entry_set(struct sit_entry_set *ses)
{
	list_del(&ses->set_list);
	kmem_cache_free(sit_entry_set_slab, ses);
}

static void adjust_sit_entry_set(struct sit_entry_set *ses,
						struct list_head *head)
{
	struct sit_entry_set *next = ses;

	if (list_is_last(&ses->set_list, head))
		return;

	list_for_each_entry_continue(next, head, set_list)
		if (ses->entry_cnt <= next->entry_cnt)
			break;

	list_move_tail(&ses->set_list, &next->set_list);
}

static void add_sit_entry(unsigned int segno, struct list_head *head)
{
	struct sit_entry_set *ses;
	unsigned int start_segno = START_SEGNO(segno);

	list_for_each_entry(ses, head, set_list) {
		if (ses->start_segno == start_segno) {
			ses->entry_cnt++;
			adjust_sit_entry_set(ses, head);
			return;
		}
	}

	ses = grab_sit_entry_set();

	ses->start_segno = start_segno;
	ses->entry_cnt++;
	list_add(&ses->set_list, head);
}

static void add_sits_in_set(struct f2fs_sb_info *sbi)
{
	struct f2fs_sm_info *sm_info = SM_I(sbi);
	struct list_head *set_list = &sm_info->sit_entry_set;
	unsigned long *bitmap = SIT_I(sbi)->dirty_sentries_bitmap;
	unsigned int segno;

	for_each_set_bit(segno, bitmap, MAIN_SEGS(sbi))
		add_sit_entry(segno, set_list);
}

static void remove_sits_in_journal(struct f2fs_sb_info *sbi)
{
	struct curseg_info *curseg = CURSEG_I(sbi, CURSEG_COLD_DATA);
	struct f2fs_journal *journal = curseg->journal;
	int i;

	down_write(&curseg->journal_rwsem);
	for (i = 0; i < sits_in_cursum(journal); i++) {
		unsigned int segno;
		bool dirtied;

		segno = le32_to_cpu(segno_in_journal(journal, i));
		dirtied = __mark_sit_entry_dirty(sbi, segno);

		if (!dirtied)
			add_sit_entry(segno, &SM_I(sbi)->sit_entry_set);
	}
	update_sits_in_cursum(journal, -i);
	up_write(&curseg->journal_rwsem);
=======
static bool flush_sits_in_journal(struct f2fs_sb_info *sbi)
{
	struct curseg_info *curseg = CURSEG_I(sbi, CURSEG_COLD_DATA);
	struct f2fs_summary_block *sum = curseg->sum_blk;
	int i;

	/*
	 * If the journal area in the current summary is full of sit entries,
	 * all the sit entries will be flushed. Otherwise the sit entries
	 * are not able to replace with newly hot sit entries.
	 */
	if (sits_in_cursum(sum) >= SIT_JOURNAL_ENTRIES) {
		for (i = sits_in_cursum(sum) - 1; i >= 0; i--) {
			unsigned int segno;
			segno = le32_to_cpu(segno_in_journal(sum, i));
			__mark_sit_entry_dirty(sbi, segno);
		}
		update_sits_in_cursum(sum, -sits_in_cursum(sum));
		return 1;
	}
	return 0;
>>>>>>> 512ca3c... stock
}

/*
 * CP calls this function, which flushes SIT entries including sit_journal,
 * and moves prefree segs to free segs.
 */
<<<<<<< HEAD
void flush_sit_entries(struct f2fs_sb_info *sbi, struct cp_control *cpc)
=======
void flush_sit_entries(struct f2fs_sb_info *sbi)
>>>>>>> 512ca3c... stock
{
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned long *bitmap = sit_i->dirty_sentries_bitmap;
	struct curseg_info *curseg = CURSEG_I(sbi, CURSEG_COLD_DATA);
<<<<<<< HEAD
	struct f2fs_journal *journal = curseg->journal;
	struct sit_entry_set *ses, *tmp;
	struct list_head *head = &SM_I(sbi)->sit_entry_set;
	bool to_journal = true;
	struct seg_entry *se;

	mutex_lock(&sit_i->sentry_lock);

	if (!sit_i->dirty_sentries)
		goto out;

	/*
	 * add and account sit entries of dirty bitmap in sit entry
	 * set temporarily
	 */
	add_sits_in_set(sbi);

	/*
	 * if there are no enough space in journal to store dirty sit
	 * entries, remove all entries from journal and add and account
	 * them in sit entry set.
	 */
	if (!__has_cursum_space(journal, sit_i->dirty_sentries, SIT_JOURNAL))
		remove_sits_in_journal(sbi);

	/*
	 * there are two steps to flush sit entries:
	 * #1, flush sit entries to journal in current cold data summary block.
	 * #2, flush sit entries to sit page.
	 */
	list_for_each_entry_safe(ses, tmp, head, set_list) {
		struct page *page = NULL;
		struct f2fs_sit_block *raw_sit = NULL;
		unsigned int start_segno = ses->start_segno;
		unsigned int end = min(start_segno + SIT_ENTRY_PER_BLOCK,
						(unsigned long)MAIN_SEGS(sbi));
		unsigned int segno = start_segno;

		if (to_journal &&
			!__has_cursum_space(journal, ses->entry_cnt, SIT_JOURNAL))
			to_journal = false;

		if (to_journal) {
			down_write(&curseg->journal_rwsem);
		} else {
			page = get_next_sit_page(sbi, start_segno);
			raw_sit = page_address(page);
		}

		/* flush dirty sit entries in region of current sit set */
		for_each_set_bit_from(segno, bitmap, end) {
			int offset, sit_offset;

			se = get_seg_entry(sbi, segno);

			/* add discard candidates */
			if (cpc->reason != CP_DISCARD) {
				cpc->trim_start = segno;
				add_discard_addrs(sbi, cpc);
			}

			if (to_journal) {
				offset = lookup_journal_in_cursum(journal,
							SIT_JOURNAL, segno, 1);
				f2fs_bug_on(sbi, offset < 0);
				segno_in_journal(journal, offset) =
							cpu_to_le32(segno);
				seg_info_to_raw_sit(se,
					&sit_in_journal(journal, offset));
			} else {
				sit_offset = SIT_ENTRY_OFFSET(sit_i, segno);
				seg_info_to_raw_sit(se,
						&raw_sit->entries[sit_offset]);
			}

			__clear_bit(segno, bitmap);
			sit_i->dirty_sentries--;
			ses->entry_cnt--;
		}

		if (to_journal)
			up_write(&curseg->journal_rwsem);
		else
			f2fs_put_page(page, 1);

		f2fs_bug_on(sbi, ses->entry_cnt);
		release_sit_entry_set(ses);
	}

	f2fs_bug_on(sbi, !list_empty(head));
	f2fs_bug_on(sbi, sit_i->dirty_sentries);
out:
	if (cpc->reason == CP_DISCARD) {
		for (; cpc->trim_start <= cpc->trim_end; cpc->trim_start++)
			add_discard_addrs(sbi, cpc);
	}
	mutex_unlock(&sit_i->sentry_lock);
=======
	struct f2fs_summary_block *sum = curseg->sum_blk;
	unsigned long nsegs = TOTAL_SEGS(sbi);
	struct page *page = NULL;
	struct f2fs_sit_block *raw_sit = NULL;
	unsigned int start = 0, end = 0;
	unsigned int segno = -1;
	bool flushed;

	mutex_lock(&curseg->curseg_mutex);
	mutex_lock(&sit_i->sentry_lock);

	/*
	 * "flushed" indicates whether sit entries in journal are flushed
	 * to the SIT area or not.
	 */
	flushed = flush_sits_in_journal(sbi);

	while ((segno = find_next_bit(bitmap, nsegs, segno + 1)) < nsegs) {
		struct seg_entry *se = get_seg_entry(sbi, segno);
		int sit_offset, offset;

		sit_offset = SIT_ENTRY_OFFSET(sit_i, segno);

		if (flushed)
			goto to_sit_page;

		offset = lookup_journal_in_cursum(sum, SIT_JOURNAL, segno, 1);
		if (offset >= 0) {
			segno_in_journal(sum, offset) = cpu_to_le32(segno);
			seg_info_to_raw_sit(se, &sit_in_journal(sum, offset));
			goto flush_done;
		}
to_sit_page:
		if (!page || (start > segno) || (segno > end)) {
			if (page) {
				f2fs_put_page(page, 1);
				page = NULL;
			}

			start = START_SEGNO(sit_i, segno);
			end = start + SIT_ENTRY_PER_BLOCK - 1;

			/* read sit block that will be updated */
			page = get_next_sit_page(sbi, start);
			raw_sit = page_address(page);
		}

		/* udpate entry in SIT block */
		seg_info_to_raw_sit(se, &raw_sit->entries[sit_offset]);
flush_done:
		__clear_bit(segno, bitmap);
		sit_i->dirty_sentries--;
	}
	mutex_unlock(&sit_i->sentry_lock);
	mutex_unlock(&curseg->curseg_mutex);

	/* writeout last modified SIT block */
	f2fs_put_page(page, 1);
>>>>>>> 512ca3c... stock

	set_prefree_as_free_segments(sbi);
}

static int build_sit_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_super = F2FS_RAW_SUPER(sbi);
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct sit_info *sit_i;
	unsigned int sit_segs, start;
	char *src_bitmap, *dst_bitmap;
	unsigned int bitmap_size;

	/* allocate memory for SIT information */
	sit_i = kzalloc(sizeof(struct sit_info), GFP_KERNEL);
	if (!sit_i)
		return -ENOMEM;

	SM_I(sbi)->sit_info = sit_i;

<<<<<<< HEAD
	sit_i->sentries = f2fs_kvzalloc(MAIN_SEGS(sbi) *
					sizeof(struct seg_entry), GFP_KERNEL);
	if (!sit_i->sentries)
		return -ENOMEM;

	bitmap_size = f2fs_bitmap_size(MAIN_SEGS(sbi));
	sit_i->dirty_sentries_bitmap = f2fs_kvzalloc(bitmap_size, GFP_KERNEL);
	if (!sit_i->dirty_sentries_bitmap)
		return -ENOMEM;

	for (start = 0; start < MAIN_SEGS(sbi); start++) {
=======
	sit_i->sentries = vzalloc(TOTAL_SEGS(sbi) * sizeof(struct seg_entry));
	if (!sit_i->sentries)
		return -ENOMEM;

	bitmap_size = f2fs_bitmap_size(TOTAL_SEGS(sbi));
	sit_i->dirty_sentries_bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!sit_i->dirty_sentries_bitmap)
		return -ENOMEM;

	for (start = 0; start < TOTAL_SEGS(sbi); start++) {
>>>>>>> 512ca3c... stock
		sit_i->sentries[start].cur_valid_map
			= kzalloc(SIT_VBLOCK_MAP_SIZE, GFP_KERNEL);
		sit_i->sentries[start].ckpt_valid_map
			= kzalloc(SIT_VBLOCK_MAP_SIZE, GFP_KERNEL);
<<<<<<< HEAD
		sit_i->sentries[start].discard_map
			= kzalloc(SIT_VBLOCK_MAP_SIZE, GFP_KERNEL);
		if (!sit_i->sentries[start].cur_valid_map ||
				!sit_i->sentries[start].ckpt_valid_map ||
				!sit_i->sentries[start].discard_map)
			return -ENOMEM;
	}

	sit_i->tmp_map = kzalloc(SIT_VBLOCK_MAP_SIZE, GFP_KERNEL);
	if (!sit_i->tmp_map)
		return -ENOMEM;

	if (sbi->segs_per_sec > 1) {
		sit_i->sec_entries = f2fs_kvzalloc(MAIN_SECS(sbi) *
					sizeof(struct sec_entry), GFP_KERNEL);
=======
		if (!sit_i->sentries[start].cur_valid_map
				|| !sit_i->sentries[start].ckpt_valid_map)
			return -ENOMEM;
	}

	if (sbi->segs_per_sec > 1) {
		sit_i->sec_entries = vzalloc(TOTAL_SECS(sbi) *
					sizeof(struct sec_entry));
>>>>>>> 512ca3c... stock
		if (!sit_i->sec_entries)
			return -ENOMEM;
	}

	/* get information related with SIT */
	sit_segs = le32_to_cpu(raw_super->segment_count_sit) >> 1;

	/* setup SIT bitmap from ckeckpoint pack */
	bitmap_size = __bitmap_size(sbi, SIT_BITMAP);
	src_bitmap = __bitmap_ptr(sbi, SIT_BITMAP);

	dst_bitmap = kmemdup(src_bitmap, bitmap_size, GFP_KERNEL);
	if (!dst_bitmap)
		return -ENOMEM;

	/* init SIT information */
	sit_i->s_ops = &default_salloc_ops;

	sit_i->sit_base_addr = le32_to_cpu(raw_super->sit_blkaddr);
	sit_i->sit_blocks = sit_segs << sbi->log_blocks_per_seg;
	sit_i->written_valid_blocks = le64_to_cpu(ckpt->valid_block_count);
	sit_i->sit_bitmap = dst_bitmap;
	sit_i->bitmap_size = bitmap_size;
	sit_i->dirty_sentries = 0;
	sit_i->sents_per_block = SIT_ENTRY_PER_BLOCK;
	sit_i->elapsed_time = le64_to_cpu(sbi->ckpt->elapsed_time);
	sit_i->mounted_time = CURRENT_TIME_SEC.tv_sec;
	mutex_init(&sit_i->sentry_lock);
	return 0;
}

static int build_free_segmap(struct f2fs_sb_info *sbi)
{
<<<<<<< HEAD
=======
	struct f2fs_sm_info *sm_info = SM_I(sbi);
>>>>>>> 512ca3c... stock
	struct free_segmap_info *free_i;
	unsigned int bitmap_size, sec_bitmap_size;

	/* allocate memory for free segmap information */
	free_i = kzalloc(sizeof(struct free_segmap_info), GFP_KERNEL);
	if (!free_i)
		return -ENOMEM;

	SM_I(sbi)->free_info = free_i;

<<<<<<< HEAD
	bitmap_size = f2fs_bitmap_size(MAIN_SEGS(sbi));
	free_i->free_segmap = f2fs_kvmalloc(bitmap_size, GFP_KERNEL);
	if (!free_i->free_segmap)
		return -ENOMEM;

	sec_bitmap_size = f2fs_bitmap_size(MAIN_SECS(sbi));
	free_i->free_secmap = f2fs_kvmalloc(sec_bitmap_size, GFP_KERNEL);
=======
	bitmap_size = f2fs_bitmap_size(TOTAL_SEGS(sbi));
	free_i->free_segmap = kmalloc(bitmap_size, GFP_KERNEL);
	if (!free_i->free_segmap)
		return -ENOMEM;

	sec_bitmap_size = f2fs_bitmap_size(TOTAL_SECS(sbi));
	free_i->free_secmap = kmalloc(sec_bitmap_size, GFP_KERNEL);
>>>>>>> 512ca3c... stock
	if (!free_i->free_secmap)
		return -ENOMEM;

	/* set all segments as dirty temporarily */
	memset(free_i->free_segmap, 0xff, bitmap_size);
	memset(free_i->free_secmap, 0xff, sec_bitmap_size);

	/* init free segmap information */
<<<<<<< HEAD
	free_i->start_segno = GET_SEGNO_FROM_SEG0(sbi, MAIN_BLKADDR(sbi));
	free_i->free_segments = 0;
	free_i->free_sections = 0;
	spin_lock_init(&free_i->segmap_lock);
=======
	free_i->start_segno =
		(unsigned int) GET_SEGNO_FROM_SEG0(sbi, sm_info->main_blkaddr);
	free_i->free_segments = 0;
	free_i->free_sections = 0;
	rwlock_init(&free_i->segmap_lock);
>>>>>>> 512ca3c... stock
	return 0;
}

static int build_curseg(struct f2fs_sb_info *sbi)
{
	struct curseg_info *array;
	int i;

<<<<<<< HEAD
	array = kcalloc(NR_CURSEG_TYPE, sizeof(*array), GFP_KERNEL);
=======
	array = kzalloc(sizeof(*array) * NR_CURSEG_TYPE, GFP_KERNEL);
>>>>>>> 512ca3c... stock
	if (!array)
		return -ENOMEM;

	SM_I(sbi)->curseg_array = array;

	for (i = 0; i < NR_CURSEG_TYPE; i++) {
		mutex_init(&array[i].curseg_mutex);
		array[i].sum_blk = kzalloc(PAGE_CACHE_SIZE, GFP_KERNEL);
		if (!array[i].sum_blk)
			return -ENOMEM;
<<<<<<< HEAD
		init_rwsem(&array[i].journal_rwsem);
		array[i].journal = kzalloc(sizeof(struct f2fs_journal),
							GFP_KERNEL);
		if (!array[i].journal)
			return -ENOMEM;
=======
>>>>>>> 512ca3c... stock
		array[i].segno = NULL_SEGNO;
		array[i].next_blkoff = 0;
	}
	return restore_curseg_summaries(sbi);
}

static void build_sit_entries(struct f2fs_sb_info *sbi)
{
	struct sit_info *sit_i = SIT_I(sbi);
	struct curseg_info *curseg = CURSEG_I(sbi, CURSEG_COLD_DATA);
<<<<<<< HEAD
	struct f2fs_journal *journal = curseg->journal;
	int sit_blk_cnt = SIT_BLK_CNT(sbi);
	unsigned int i, start, end;
	unsigned int readed, start_blk = 0;
	int nrpages = MAX_BIO_BLOCKS(sbi) * 8;

	do {
		readed = ra_meta_pages(sbi, start_blk, nrpages, META_SIT, true);

		start = start_blk * sit_i->sents_per_block;
		end = (start_blk + readed) * sit_i->sents_per_block;

		for (; start < end && start < MAIN_SEGS(sbi); start++) {
			struct seg_entry *se = &sit_i->sentries[start];
			struct f2fs_sit_block *sit_blk;
			struct f2fs_sit_entry sit;
			struct page *page;

			down_read(&curseg->journal_rwsem);
			for (i = 0; i < sits_in_cursum(journal); i++) {
				if (le32_to_cpu(segno_in_journal(journal, i))
								== start) {
					sit = sit_in_journal(journal, i);
					up_read(&curseg->journal_rwsem);
					goto got_it;
				}
			}
			up_read(&curseg->journal_rwsem);

			page = get_current_sit_page(sbi, start);
			sit_blk = (struct f2fs_sit_block *)page_address(page);
			sit = sit_blk->entries[SIT_ENTRY_OFFSET(sit_i, start)];
			f2fs_put_page(page, 1);
got_it:
			check_block_count(sbi, start, &sit);
			seg_info_from_raw_sit(se, &sit);

			/* build discard map only one time */
			memcpy(se->discard_map, se->cur_valid_map, SIT_VBLOCK_MAP_SIZE);
			sbi->discard_blks += sbi->blocks_per_seg - se->valid_blocks;

			if (sbi->segs_per_sec > 1) {
				struct sec_entry *e = get_sec_entry(sbi, start);
				e->valid_blocks += se->valid_blocks;
			}
		}
		start_blk += readed;
	} while (start_blk < sit_blk_cnt);
=======
	struct f2fs_summary_block *sum = curseg->sum_blk;
	unsigned int start;

	for (start = 0; start < TOTAL_SEGS(sbi); start++) {
		struct seg_entry *se = &sit_i->sentries[start];
		struct f2fs_sit_block *sit_blk;
		struct f2fs_sit_entry sit;
		struct page *page;
		int i;

		mutex_lock(&curseg->curseg_mutex);
		for (i = 0; i < sits_in_cursum(sum); i++) {
			if (le32_to_cpu(segno_in_journal(sum, i)) == start) {
				sit = sit_in_journal(sum, i);
				mutex_unlock(&curseg->curseg_mutex);
				goto got_it;
			}
		}
		mutex_unlock(&curseg->curseg_mutex);
		page = get_current_sit_page(sbi, start);
		sit_blk = (struct f2fs_sit_block *)page_address(page);
		sit = sit_blk->entries[SIT_ENTRY_OFFSET(sit_i, start)];
		f2fs_put_page(page, 1);
got_it:
		check_block_count(sbi, start, &sit);
		seg_info_from_raw_sit(se, &sit);
		if (sbi->segs_per_sec > 1) {
			struct sec_entry *e = get_sec_entry(sbi, start);
			e->valid_blocks += se->valid_blocks;
		}
	}
>>>>>>> 512ca3c... stock
}

static void init_free_segmap(struct f2fs_sb_info *sbi)
{
	unsigned int start;
	int type;

<<<<<<< HEAD
	for (start = 0; start < MAIN_SEGS(sbi); start++) {
=======
	for (start = 0; start < TOTAL_SEGS(sbi); start++) {
>>>>>>> 512ca3c... stock
		struct seg_entry *sentry = get_seg_entry(sbi, start);
		if (!sentry->valid_blocks)
			__set_free(sbi, start);
	}

	/* set use the current segments */
	for (type = CURSEG_HOT_DATA; type <= CURSEG_COLD_NODE; type++) {
		struct curseg_info *curseg_t = CURSEG_I(sbi, type);
		__set_test_and_inuse(sbi, curseg_t->segno);
	}
}

static void init_dirty_segmap(struct f2fs_sb_info *sbi)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	struct free_segmap_info *free_i = FREE_I(sbi);
	unsigned int segno = 0, offset = 0;
	unsigned short valid_blocks;

<<<<<<< HEAD
	while (1) {
		/* find dirty segment based on free segmap */
		segno = find_next_inuse(free_i, MAIN_SEGS(sbi), offset);
		if (segno >= MAIN_SEGS(sbi))
			break;
		offset = segno + 1;
		valid_blocks = get_valid_blocks(sbi, segno, 0);
		if (valid_blocks == sbi->blocks_per_seg || !valid_blocks)
			continue;
		if (valid_blocks > sbi->blocks_per_seg) {
			f2fs_bug_on(sbi, 1);
			continue;
		}
=======
	while (segno < TOTAL_SEGS(sbi)) {
		/* find dirty segment based on free segmap */
		segno = find_next_inuse(free_i, TOTAL_SEGS(sbi), offset);
		if (segno >= TOTAL_SEGS(sbi))
			break;
		offset = segno + 1;
		valid_blocks = get_valid_blocks(sbi, segno, 0);
		if (valid_blocks >= sbi->blocks_per_seg || !valid_blocks)
			continue;
>>>>>>> 512ca3c... stock
		mutex_lock(&dirty_i->seglist_lock);
		__locate_dirty_segment(sbi, segno, DIRTY);
		mutex_unlock(&dirty_i->seglist_lock);
	}
}

static int init_victim_secmap(struct f2fs_sb_info *sbi)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
<<<<<<< HEAD
	unsigned int bitmap_size = f2fs_bitmap_size(MAIN_SECS(sbi));

	dirty_i->victim_secmap = f2fs_kvzalloc(bitmap_size, GFP_KERNEL);
=======
	unsigned int bitmap_size = f2fs_bitmap_size(TOTAL_SECS(sbi));

	dirty_i->victim_secmap = kzalloc(bitmap_size, GFP_KERNEL);
>>>>>>> 512ca3c... stock
	if (!dirty_i->victim_secmap)
		return -ENOMEM;
	return 0;
}

static int build_dirty_segmap(struct f2fs_sb_info *sbi)
{
	struct dirty_seglist_info *dirty_i;
	unsigned int bitmap_size, i;

	/* allocate memory for dirty segments list information */
	dirty_i = kzalloc(sizeof(struct dirty_seglist_info), GFP_KERNEL);
	if (!dirty_i)
		return -ENOMEM;

	SM_I(sbi)->dirty_info = dirty_i;
	mutex_init(&dirty_i->seglist_lock);

<<<<<<< HEAD
	bitmap_size = f2fs_bitmap_size(MAIN_SEGS(sbi));

	for (i = 0; i < NR_DIRTY_TYPE; i++) {
		dirty_i->dirty_segmap[i] = f2fs_kvzalloc(bitmap_size, GFP_KERNEL);
=======
	bitmap_size = f2fs_bitmap_size(TOTAL_SEGS(sbi));

	for (i = 0; i < NR_DIRTY_TYPE; i++) {
		dirty_i->dirty_segmap[i] = kzalloc(bitmap_size, GFP_KERNEL);
>>>>>>> 512ca3c... stock
		if (!dirty_i->dirty_segmap[i])
			return -ENOMEM;
	}

	init_dirty_segmap(sbi);
	return init_victim_secmap(sbi);
}

/*
 * Update min, max modified time for cost-benefit GC algorithm
 */
static void init_min_max_mtime(struct f2fs_sb_info *sbi)
{
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned int segno;

	mutex_lock(&sit_i->sentry_lock);

	sit_i->min_mtime = LLONG_MAX;

<<<<<<< HEAD
	for (segno = 0; segno < MAIN_SEGS(sbi); segno += sbi->segs_per_sec) {
=======
	for (segno = 0; segno < TOTAL_SEGS(sbi); segno += sbi->segs_per_sec) {
>>>>>>> 512ca3c... stock
		unsigned int i;
		unsigned long long mtime = 0;

		for (i = 0; i < sbi->segs_per_sec; i++)
			mtime += get_seg_entry(sbi, segno + i)->mtime;

		mtime = div_u64(mtime, sbi->segs_per_sec);

		if (sit_i->min_mtime > mtime)
			sit_i->min_mtime = mtime;
	}
	sit_i->max_mtime = get_mtime(sbi);
	mutex_unlock(&sit_i->sentry_lock);
}

int build_segment_manager(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_super = F2FS_RAW_SUPER(sbi);
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct f2fs_sm_info *sm_info;
	int err;

	sm_info = kzalloc(sizeof(struct f2fs_sm_info), GFP_KERNEL);
	if (!sm_info)
		return -ENOMEM;

	/* init sm info */
	sbi->sm_info = sm_info;
<<<<<<< HEAD
=======
	INIT_LIST_HEAD(&sm_info->wblist_head);
	spin_lock_init(&sm_info->wblist_lock);
>>>>>>> 512ca3c... stock
	sm_info->seg0_blkaddr = le32_to_cpu(raw_super->segment0_blkaddr);
	sm_info->main_blkaddr = le32_to_cpu(raw_super->main_blkaddr);
	sm_info->segment_count = le32_to_cpu(raw_super->segment_count);
	sm_info->reserved_segments = le32_to_cpu(ckpt->rsvd_segment_count);
	sm_info->ovp_segments = le32_to_cpu(ckpt->overprov_segment_count);
	sm_info->main_segments = le32_to_cpu(raw_super->segment_count_main);
	sm_info->ssa_blkaddr = le32_to_cpu(raw_super->ssa_blkaddr);
<<<<<<< HEAD
	sm_info->rec_prefree_segments = sm_info->main_segments *
					DEF_RECLAIM_PREFREE_SEGMENTS / 100;
	sm_info->ipu_policy = 1 << F2FS_IPU_FSYNC;
	sm_info->min_ipu_util = DEF_MIN_IPU_UTIL;
	sm_info->min_fsync_blocks = DEF_MIN_FSYNC_BLOCKS;

	INIT_LIST_HEAD(&sm_info->discard_list);
	sm_info->nr_discards = 0;
	sm_info->max_discards = 0;

	sm_info->trim_sections = DEF_BATCHED_TRIM_SECTIONS;

	INIT_LIST_HEAD(&sm_info->sit_entry_set);

	if (test_opt(sbi, FLUSH_MERGE) && !f2fs_readonly(sbi->sb)) {
		err = create_flush_cmd_control(sbi);
		if (err)
			return err;
	}
=======
>>>>>>> 512ca3c... stock

	err = build_sit_info(sbi);
	if (err)
		return err;
	err = build_free_segmap(sbi);
	if (err)
		return err;
	err = build_curseg(sbi);
	if (err)
		return err;

	/* reinit free segmap based on SIT */
	build_sit_entries(sbi);

	init_free_segmap(sbi);
	err = build_dirty_segmap(sbi);
	if (err)
		return err;

	init_min_max_mtime(sbi);
	return 0;
}

static void discard_dirty_segmap(struct f2fs_sb_info *sbi,
		enum dirty_type dirty_type)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);

	mutex_lock(&dirty_i->seglist_lock);
<<<<<<< HEAD
	f2fs_kvfree(dirty_i->dirty_segmap[dirty_type]);
=======
	kfree(dirty_i->dirty_segmap[dirty_type]);
>>>>>>> 512ca3c... stock
	dirty_i->nr_dirty[dirty_type] = 0;
	mutex_unlock(&dirty_i->seglist_lock);
}

static void destroy_victim_secmap(struct f2fs_sb_info *sbi)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
<<<<<<< HEAD
	f2fs_kvfree(dirty_i->victim_secmap);
=======
	kfree(dirty_i->victim_secmap);
>>>>>>> 512ca3c... stock
}

static void destroy_dirty_segmap(struct f2fs_sb_info *sbi)
{
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	int i;

	if (!dirty_i)
		return;

	/* discard pre-free/dirty segments list */
	for (i = 0; i < NR_DIRTY_TYPE; i++)
		discard_dirty_segmap(sbi, i);

	destroy_victim_secmap(sbi);
	SM_I(sbi)->dirty_info = NULL;
	kfree(dirty_i);
}

static void destroy_curseg(struct f2fs_sb_info *sbi)
{
	struct curseg_info *array = SM_I(sbi)->curseg_array;
	int i;

	if (!array)
		return;
	SM_I(sbi)->curseg_array = NULL;
<<<<<<< HEAD
	for (i = 0; i < NR_CURSEG_TYPE; i++) {
		kfree(array[i].sum_blk);
		kfree(array[i].journal);
	}
=======
	for (i = 0; i < NR_CURSEG_TYPE; i++)
		kfree(array[i].sum_blk);
>>>>>>> 512ca3c... stock
	kfree(array);
}

static void destroy_free_segmap(struct f2fs_sb_info *sbi)
{
	struct free_segmap_info *free_i = SM_I(sbi)->free_info;
	if (!free_i)
		return;
	SM_I(sbi)->free_info = NULL;
<<<<<<< HEAD
	f2fs_kvfree(free_i->free_segmap);
	f2fs_kvfree(free_i->free_secmap);
=======
	kfree(free_i->free_segmap);
	kfree(free_i->free_secmap);
>>>>>>> 512ca3c... stock
	kfree(free_i);
}

static void destroy_sit_info(struct f2fs_sb_info *sbi)
{
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned int start;

	if (!sit_i)
		return;

	if (sit_i->sentries) {
<<<<<<< HEAD
		for (start = 0; start < MAIN_SEGS(sbi); start++) {
			kfree(sit_i->sentries[start].cur_valid_map);
			kfree(sit_i->sentries[start].ckpt_valid_map);
			kfree(sit_i->sentries[start].discard_map);
		}
	}
	kfree(sit_i->tmp_map);

	f2fs_kvfree(sit_i->sentries);
	f2fs_kvfree(sit_i->sec_entries);
	f2fs_kvfree(sit_i->dirty_sentries_bitmap);
=======
		for (start = 0; start < TOTAL_SEGS(sbi); start++) {
			kfree(sit_i->sentries[start].cur_valid_map);
			kfree(sit_i->sentries[start].ckpt_valid_map);
		}
	}
	vfree(sit_i->sentries);
	vfree(sit_i->sec_entries);
	kfree(sit_i->dirty_sentries_bitmap);
>>>>>>> 512ca3c... stock

	SM_I(sbi)->sit_info = NULL;
	kfree(sit_i->sit_bitmap);
	kfree(sit_i);
}

void destroy_segment_manager(struct f2fs_sb_info *sbi)
{
	struct f2fs_sm_info *sm_info = SM_I(sbi);
<<<<<<< HEAD

	if (!sm_info)
		return;
	destroy_flush_cmd_control(sbi);
=======
>>>>>>> 512ca3c... stock
	destroy_dirty_segmap(sbi);
	destroy_curseg(sbi);
	destroy_free_segmap(sbi);
	destroy_sit_info(sbi);
	sbi->sm_info = NULL;
	kfree(sm_info);
}
<<<<<<< HEAD

int __init create_segment_manager_caches(void)
{
	discard_entry_slab = f2fs_kmem_cache_create("discard_entry",
			sizeof(struct discard_entry));
	if (!discard_entry_slab)
		goto fail;

	sit_entry_set_slab = f2fs_kmem_cache_create("sit_entry_set",
			sizeof(struct sit_entry_set));
	if (!sit_entry_set_slab)
		goto destory_discard_entry;

	inmem_entry_slab = f2fs_kmem_cache_create("inmem_page_entry",
			sizeof(struct inmem_pages));
	if (!inmem_entry_slab)
		goto destroy_sit_entry_set;
	return 0;

destroy_sit_entry_set:
	kmem_cache_destroy(sit_entry_set_slab);
destory_discard_entry:
	kmem_cache_destroy(discard_entry_slab);
fail:
	return -ENOMEM;
}

void destroy_segment_manager_caches(void)
{
	kmem_cache_destroy(sit_entry_set_slab);
	kmem_cache_destroy(discard_entry_slab);
	kmem_cache_destroy(inmem_entry_slab);
}
=======
>>>>>>> 512ca3c... stock
