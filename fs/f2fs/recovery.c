/*
 * fs/f2fs/recovery.c
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
#include "f2fs.h"
#include "node.h"
#include "segment.h"

<<<<<<< HEAD
/*
 * Roll forward recovery scenarios.
 *
 * [Term] F: fsync_mark, D: dentry_mark
 *
 * 1. inode(x) | CP | inode(x) | dnode(F)
 * -> Update the latest inode(x).
 *
 * 2. inode(x) | CP | inode(F) | dnode(F)
 * -> No problem.
 *
 * 3. inode(x) | CP | dnode(F) | inode(x)
 * -> Recover to the latest dnode(F), and drop the last inode(x)
 *
 * 4. inode(x) | CP | dnode(F) | inode(F)
 * -> No problem.
 *
 * 5. CP | inode(x) | dnode(F)
 * -> The inode(DF) was missing. Should drop this dnode(F).
 *
 * 6. CP | inode(DF) | dnode(F)
 * -> No problem.
 *
 * 7. CP | dnode(F) | inode(DF)
 * -> If f2fs_iget fails, then goto next to find inode(DF).
 *
 * 8. CP | dnode(F) | inode(x)
 * -> If f2fs_iget fails, then goto next to find inode(DF).
 *    But it will fail due to no inode(DF).
 */

=======
>>>>>>> 512ca3c... stock
static struct kmem_cache *fsync_entry_slab;

bool space_for_roll_forward(struct f2fs_sb_info *sbi)
{
	if (sbi->last_valid_block_count + sbi->alloc_valid_block_count
			> sbi->user_block_count)
		return false;
	return true;
}

static struct fsync_inode_entry *get_fsync_inode(struct list_head *head,
								nid_t ino)
{
<<<<<<< HEAD
	struct fsync_inode_entry *entry;

	list_for_each_entry(entry, head, list)
		if (entry->inode->i_ino == ino)
			return entry;

	return NULL;
}

static int recover_dentry(struct inode *inode, struct page *ipage)
{
	struct f2fs_inode *raw_inode = F2FS_INODE(ipage);
	nid_t pino = le32_to_cpu(raw_inode->i_pino);
	struct f2fs_dir_entry *de;
	struct qstr name;
	struct page *page;
	struct inode *dir, *einode;
	int err = 0;

	dir = f2fs_iget(inode->i_sb, pino);
=======
	struct list_head *this;
	struct fsync_inode_entry *entry;

	list_for_each(this, head) {
		entry = list_entry(this, struct fsync_inode_entry, list);
		if (entry->inode->i_ino == ino)
			return entry;
	}
	return NULL;
}

static int recover_dentry(struct page *ipage, struct inode *inode)
{
	struct f2fs_node *raw_node = (struct f2fs_node *)kmap(ipage);
	struct f2fs_inode *raw_inode = &(raw_node->i);
	struct qstr name;
	struct f2fs_dir_entry *de;
	struct page *page;
	struct inode *dir;
	int err = 0;

	if (!is_dent_dnode(ipage))
		goto out;

	dir = f2fs_iget(inode->i_sb, le32_to_cpu(raw_inode->i_pino));
>>>>>>> 512ca3c... stock
	if (IS_ERR(dir)) {
		err = PTR_ERR(dir);
		goto out;
	}

<<<<<<< HEAD
	if (file_enc_name(inode)) {
		iput(dir);
		return 0;
	}

	name.len = le32_to_cpu(raw_inode->i_namelen);
	name.name = raw_inode->i_name;

	if (unlikely(name.len > F2FS_NAME_LEN)) {
		WARN_ON(1);
		err = -ENAMETOOLONG;
		goto out_err;
	}
retry:
	de = f2fs_find_entry(dir, &name, &page);
	if (de && inode->i_ino == le32_to_cpu(de->ino))
		goto out_unmap_put;

	if (de) {
		einode = f2fs_iget(inode->i_sb, le32_to_cpu(de->ino));
		if (IS_ERR(einode)) {
			WARN_ON(1);
			err = PTR_ERR(einode);
			if (err == -ENOENT)
				err = -EEXIST;
			goto out_unmap_put;
		}
		err = acquire_orphan_inode(F2FS_I_SB(inode));
		if (err) {
			iput(einode);
			goto out_unmap_put;
		}
		f2fs_delete_entry(de, page, dir, einode);
		iput(einode);
		goto retry;
	}
	err = __f2fs_add_link(dir, &name, inode, inode->i_ino, inode->i_mode);
	if (err)
		goto out_err;

	if (is_inode_flag_set(F2FS_I(dir), FI_DELAY_IPUT)) {
		iput(dir);
	} else {
		add_dirty_dir_inode(dir);
		set_inode_flag(F2FS_I(dir), FI_DELAY_IPUT);
	}

	goto out;

out_unmap_put:
	f2fs_dentry_kunmap(dir, page);
	f2fs_put_page(page, 0);
out_err:
	iput(dir);
out:
	f2fs_msg(inode->i_sb, KERN_NOTICE,
			"%s: ino = %x, name = %s, dir = %lx, err = %d",
			__func__, ino_of_node(ipage), raw_inode->i_name,
			IS_ERR(dir) ? 0 : dir->i_ino, err);
	return err;
}

static void recover_inode(struct inode *inode, struct page *page)
{
	struct f2fs_inode *raw = F2FS_INODE(page);
	char *name;

	inode->i_mode = le16_to_cpu(raw->i_mode);
	i_size_write(inode, le64_to_cpu(raw->i_size));
	inode->i_atime.tv_sec = le64_to_cpu(raw->i_mtime);
	inode->i_ctime.tv_sec = le64_to_cpu(raw->i_ctime);
	inode->i_mtime.tv_sec = le64_to_cpu(raw->i_mtime);
	inode->i_atime.tv_nsec = le32_to_cpu(raw->i_mtime_nsec);
	inode->i_ctime.tv_nsec = le32_to_cpu(raw->i_ctime_nsec);
	inode->i_mtime.tv_nsec = le32_to_cpu(raw->i_mtime_nsec);

	if (file_enc_name(inode))
		name = "<encrypted>";
	else
		name = F2FS_INODE(page)->i_name;

	f2fs_msg(inode->i_sb, KERN_NOTICE, "recover_inode: ino = %x, name = %s",
			ino_of_node(page), name);
}

static bool is_same_inode(struct inode *inode, struct page *ipage)
{
	struct f2fs_inode *ri = F2FS_INODE(ipage);
	struct timespec disk;

	if (!IS_INODE(ipage))
		return true;

	disk.tv_sec = le64_to_cpu(ri->i_ctime);
	disk.tv_nsec = le32_to_cpu(ri->i_ctime_nsec);
	if (timespec_compare(&inode->i_ctime, &disk) > 0)
		return false;

	disk.tv_sec = le64_to_cpu(ri->i_atime);
	disk.tv_nsec = le32_to_cpu(ri->i_atime_nsec);
	if (timespec_compare(&inode->i_atime, &disk) > 0)
		return false;

	disk.tv_sec = le64_to_cpu(ri->i_mtime);
	disk.tv_nsec = le32_to_cpu(ri->i_mtime_nsec);
	if (timespec_compare(&inode->i_mtime, &disk) > 0)
		return false;

	return true;
=======
	name.len = le32_to_cpu(raw_inode->i_namelen);
	name.name = raw_inode->i_name;

	de = f2fs_find_entry(dir, &name, &page);
	if (de) {
		kunmap(page);
		f2fs_put_page(page, 0);
	} else {
		err = __f2fs_add_link(dir, &name, inode);
	}
	iput(dir);
out:
	kunmap(ipage);
	return err;
}

static int recover_inode(struct inode *inode, struct page *node_page)
{
	void *kaddr = page_address(node_page);
	struct f2fs_node *raw_node = (struct f2fs_node *)kaddr;
	struct f2fs_inode *raw_inode = &(raw_node->i);

	inode->i_mode = le16_to_cpu(raw_inode->i_mode);
	i_size_write(inode, le64_to_cpu(raw_inode->i_size));
	inode->i_atime.tv_sec = le64_to_cpu(raw_inode->i_mtime);
	inode->i_ctime.tv_sec = le64_to_cpu(raw_inode->i_ctime);
	inode->i_mtime.tv_sec = le64_to_cpu(raw_inode->i_mtime);
	inode->i_atime.tv_nsec = le32_to_cpu(raw_inode->i_mtime_nsec);
	inode->i_ctime.tv_nsec = le32_to_cpu(raw_inode->i_ctime_nsec);
	inode->i_mtime.tv_nsec = le32_to_cpu(raw_inode->i_mtime_nsec);

	return recover_dentry(node_page, inode);
>>>>>>> 512ca3c... stock
}

static int find_fsync_dnodes(struct f2fs_sb_info *sbi, struct list_head *head)
{
<<<<<<< HEAD
	unsigned long long cp_ver = cur_cp_version(F2FS_CKPT(sbi));
	struct curseg_info *curseg;
	struct page *page = NULL;
=======
	unsigned long long cp_ver = le64_to_cpu(sbi->ckpt->checkpoint_ver);
	struct curseg_info *curseg;
	struct page *page;
>>>>>>> 512ca3c... stock
	block_t blkaddr;
	int err = 0;

	/* get node pages in the current segment */
	curseg = CURSEG_I(sbi, CURSEG_WARM_NODE);
<<<<<<< HEAD
	blkaddr = NEXT_FREE_BLKADDR(sbi, curseg);

	ra_meta_pages(sbi, blkaddr, 1, META_POR, true);
=======
	blkaddr = START_BLOCK(sbi, curseg->segno) + curseg->next_blkoff;

	/* read node page */
	page = alloc_page(GFP_F2FS_ZERO);
	if (IS_ERR(page))
		return PTR_ERR(page);
	lock_page(page);
>>>>>>> 512ca3c... stock

	while (1) {
		struct fsync_inode_entry *entry;

<<<<<<< HEAD
		if (!is_valid_blkaddr(sbi, blkaddr, META_POR))
			return 0;

		page = get_tmp_page(sbi, blkaddr);

		if (cp_ver != cpver_of_node(page))
			break;
=======
		err = f2fs_readpage(sbi, page, blkaddr, READ_SYNC);
		if (err)
			goto out;

		lock_page(page);

		if (cp_ver != cpver_of_node(page))
			goto unlock_out;
>>>>>>> 512ca3c... stock

		if (!is_fsync_dnode(page))
			goto next;

		entry = get_fsync_inode(head, ino_of_node(page));
		if (entry) {
<<<<<<< HEAD
			if (!is_same_inode(entry->inode, page))
				goto next;
=======
			entry->blkaddr = blkaddr;
			if (IS_INODE(page) && is_dent_dnode(page))
				set_inode_flag(F2FS_I(entry->inode),
							FI_INC_LINK);
>>>>>>> 512ca3c... stock
		} else {
			if (IS_INODE(page) && is_dent_dnode(page)) {
				err = recover_inode_page(sbi, page);
				if (err)
<<<<<<< HEAD
					break;
			}

			/* add this fsync inode to the list */
			entry = kmem_cache_alloc(fsync_entry_slab, GFP_F2FS_ZERO);
			if (!entry) {
				err = -ENOMEM;
				break;
			}
			/*
			 * CP | dnode(F) | inode(DF)
			 * For this case, we should not give up now.
			 */
=======
					goto unlock_out;
			}

			/* add this fsync inode to the list */
			entry = kmem_cache_alloc(fsync_entry_slab, GFP_NOFS);
			if (!entry) {
				err = -ENOMEM;
				goto unlock_out;
			}

>>>>>>> 512ca3c... stock
			entry->inode = f2fs_iget(sbi->sb, ino_of_node(page));
			if (IS_ERR(entry->inode)) {
				err = PTR_ERR(entry->inode);
				kmem_cache_free(fsync_entry_slab, entry);
<<<<<<< HEAD
				if (err == -ENOENT) {
					err = 0;
					goto next;
				}
				break;
			}
			list_add_tail(&entry->list, head);
		}
		entry->blkaddr = blkaddr;

		if (IS_INODE(page)) {
			entry->last_inode = blkaddr;
			if (is_dent_dnode(page))
				entry->last_dentry = blkaddr;
=======
				goto unlock_out;
			}

			list_add_tail(&entry->list, head);
			entry->blkaddr = blkaddr;
		}
		if (IS_INODE(page)) {
			err = recover_inode(entry->inode, page);
			if (err == -ENOENT) {
				goto next;
			} else if (err) {
				err = -EINVAL;
				goto unlock_out;
			}
>>>>>>> 512ca3c... stock
		}
next:
		/* check next segment */
		blkaddr = next_blkaddr_of_node(page);
<<<<<<< HEAD
		f2fs_put_page(page, 1);

		ra_meta_pages_cond(sbi, blkaddr);
	}
	f2fs_put_page(page, 1);
	return err;
}

static void destroy_fsync_dnodes(struct list_head *head)
=======
	}
unlock_out:
	unlock_page(page);
out:
	__free_pages(page, 0);
	return err;
}

static void destroy_fsync_dnodes(struct f2fs_sb_info *sbi,
					struct list_head *head)
>>>>>>> 512ca3c... stock
{
	struct fsync_inode_entry *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, head, list) {
		iput(entry->inode);
		list_del(&entry->list);
		kmem_cache_free(fsync_entry_slab, entry);
	}
}

<<<<<<< HEAD
static int check_index_in_prev_nodes(struct f2fs_sb_info *sbi,
			block_t blkaddr, struct dnode_of_data *dn)
{
	struct seg_entry *sentry;
	unsigned int segno = GET_SEGNO(sbi, blkaddr);
	unsigned short blkoff = GET_BLKOFF_FROM_SEG0(sbi, blkaddr);
	struct f2fs_summary_block *sum_node;
	struct f2fs_summary sum;
	struct page *sum_page, *node_page;
	struct dnode_of_data tdn = *dn;
	nid_t ino, nid;
	struct inode *inode;
	unsigned int offset;
=======
static void check_index_in_prev_nodes(struct f2fs_sb_info *sbi,
						block_t blkaddr)
{
	struct seg_entry *sentry;
	unsigned int segno = GET_SEGNO(sbi, blkaddr);
	unsigned short blkoff = GET_SEGOFF_FROM_SEG0(sbi, blkaddr) &
					(sbi->blocks_per_seg - 1);
	struct f2fs_summary sum;
	nid_t ino;
	void *kaddr;
	struct inode *inode;
	struct page *node_page;
>>>>>>> 512ca3c... stock
	block_t bidx;
	int i;

	sentry = get_seg_entry(sbi, segno);
	if (!f2fs_test_bit(blkoff, sentry->cur_valid_map))
<<<<<<< HEAD
		return 0;
=======
		return;
>>>>>>> 512ca3c... stock

	/* Get the previous summary */
	for (i = CURSEG_WARM_DATA; i <= CURSEG_COLD_DATA; i++) {
		struct curseg_info *curseg = CURSEG_I(sbi, i);
		if (curseg->segno == segno) {
			sum = curseg->sum_blk->entries[blkoff];
<<<<<<< HEAD
			goto got_it;
		}
	}

	sum_page = get_sum_page(sbi, segno);
	sum_node = (struct f2fs_summary_block *)page_address(sum_page);
	sum = sum_node->entries[blkoff];
	f2fs_put_page(sum_page, 1);
got_it:
	/* Use the locked dnode page and inode */
	nid = le32_to_cpu(sum.nid);
	if (dn->inode->i_ino == nid) {
		tdn.nid = nid;
		if (!dn->inode_page_locked)
			lock_page(dn->inode_page);
		tdn.node_page = dn->inode_page;
		tdn.ofs_in_node = le16_to_cpu(sum.ofs_in_node);
		goto truncate_out;
	} else if (dn->nid == nid) {
		tdn.ofs_in_node = le16_to_cpu(sum.ofs_in_node);
		goto truncate_out;
	}

	/* Get the node page */
	node_page = get_node_page(sbi, nid);
	if (IS_ERR(node_page))
		return PTR_ERR(node_page);

	offset = ofs_of_node(node_page);
	ino = ino_of_node(node_page);
	f2fs_put_page(node_page, 1);

	if (ino != dn->inode->i_ino) {
		/* Deallocate previous index in the node page */
		inode = f2fs_iget(sbi->sb, ino);
		if (IS_ERR(inode))
			return PTR_ERR(inode);
	} else {
		inode = dn->inode;
	}

	bidx = start_bidx_of_node(offset, inode) + le16_to_cpu(sum.ofs_in_node);

	/*
	 * if inode page is locked, unlock temporarily, but its reference
	 * count keeps alive.
	 */
	if (ino == dn->inode->i_ino && dn->inode_page_locked)
		unlock_page(dn->inode_page);

	set_new_dnode(&tdn, inode, NULL, NULL, 0);
	if (get_dnode_of_data(&tdn, bidx, LOOKUP_NODE))
		goto out;

	if (tdn.data_blkaddr == blkaddr)
		truncate_data_blocks_range(&tdn, 1);

	f2fs_put_dnode(&tdn);
out:
	if (ino != dn->inode->i_ino)
		iput(inode);
	else if (dn->inode_page_locked)
		lock_page(dn->inode_page);
	return 0;

truncate_out:
	if (datablock_addr(tdn.node_page, tdn.ofs_in_node) == blkaddr)
		truncate_data_blocks_range(&tdn, 1);
	if (dn->inode->i_ino == nid && !dn->inode_page_locked)
		unlock_page(dn->inode_page);
	return 0;
=======
			break;
		}
	}
	if (i > CURSEG_COLD_DATA) {
		struct page *sum_page = get_sum_page(sbi, segno);
		struct f2fs_summary_block *sum_node;
		kaddr = page_address(sum_page);
		sum_node = (struct f2fs_summary_block *)kaddr;
		sum = sum_node->entries[blkoff];
		f2fs_put_page(sum_page, 1);
	}

	/* Get the node page */
	node_page = get_node_page(sbi, le32_to_cpu(sum.nid));
	bidx = start_bidx_of_node(ofs_of_node(node_page)) +
				le16_to_cpu(sum.ofs_in_node);
	ino = ino_of_node(node_page);
	f2fs_put_page(node_page, 1);

	/* Deallocate previous index in the node page */
	inode = f2fs_iget(sbi->sb, ino);
	if (IS_ERR(inode))
		return;

	truncate_hole(inode, bidx, bidx + 1);
	iput(inode);
>>>>>>> 512ca3c... stock
}

static int do_recover_data(struct f2fs_sb_info *sbi, struct inode *inode,
					struct page *page, block_t blkaddr)
{
<<<<<<< HEAD
	struct dnode_of_data dn;
	struct node_info ni;
	unsigned int start, end;
	int err = 0, recovered = 0;

	/* step 1: recover xattr */
	if (IS_INODE(page)) {
		recover_inline_xattr(inode, page);
	} else if (f2fs_has_xattr_block(ofs_of_node(page))) {
		/*
		 * Deprecated; xattr blocks should be found from cold log.
		 * But, we should remain this for backward compatibility.
		 */
		recover_xattr_data(inode, page, blkaddr);
		goto out;
	}

	/* step 2: recover inline data */
	if (recover_inline_data(inode, page))
		goto out;

	/* step 3: recover data indices */
	start = start_bidx_of_node(ofs_of_node(page), inode);
	end = start + ADDRS_PER_PAGE(page, inode);

	set_new_dnode(&dn, inode, NULL, NULL, 0);

	err = get_dnode_of_data(&dn, start, ALLOC_NODE);
	if (err)
		goto out;

	f2fs_wait_on_page_writeback(dn.node_page, NODE, true);

	get_node_info(sbi, dn.nid, &ni);
	f2fs_bug_on(sbi, ni.ino != ino_of_node(page));
	f2fs_bug_on(sbi, ofs_of_node(dn.node_page) != ofs_of_node(page));

	for (; start < end; start++, dn.ofs_in_node++) {
=======
	unsigned int start, end;
	struct dnode_of_data dn;
	struct f2fs_summary sum;
	struct node_info ni;
	int err = 0;
	int ilock;

	start = start_bidx_of_node(ofs_of_node(page));
	if (IS_INODE(page))
		end = start + ADDRS_PER_INODE;
	else
		end = start + ADDRS_PER_BLOCK;

	ilock = mutex_lock_op(sbi);
	set_new_dnode(&dn, inode, NULL, NULL, 0);

	err = get_dnode_of_data(&dn, start, ALLOC_NODE);
	if (err) {
		mutex_unlock_op(sbi, ilock);
		return err;
	}

	wait_on_page_writeback(dn.node_page);

	get_node_info(sbi, dn.nid, &ni);
	BUG_ON(ni.ino != ino_of_node(page));
	BUG_ON(ofs_of_node(dn.node_page) != ofs_of_node(page));

	for (; start < end; start++) {
>>>>>>> 512ca3c... stock
		block_t src, dest;

		src = datablock_addr(dn.node_page, dn.ofs_in_node);
		dest = datablock_addr(page, dn.ofs_in_node);

<<<<<<< HEAD
		/* skip recovering if dest is the same as src */
		if (src == dest)
			continue;

		/* dest is invalid, just invalidate src block */
		if (dest == NULL_ADDR) {
			truncate_data_blocks_range(&dn, 1);
			continue;
		}

		/*
		 * dest is reserved block, invalidate src block
		 * and then reserve one new block in dnode page.
		 */
		if (dest == NEW_ADDR) {
			truncate_data_blocks_range(&dn, 1);
			err = reserve_new_block(&dn);
			f2fs_bug_on(sbi, err);
			continue;
		}

		/* dest is valid block, try to recover from src to dest */
		if (is_valid_blkaddr(sbi, dest, META_POR)) {

			if (src == NULL_ADDR) {
				err = reserve_new_block(&dn);
				/* We should not get -ENOSPC */
				f2fs_bug_on(sbi, err);
			}

			/* Check the previous node page having this index */
			err = check_index_in_prev_nodes(sbi, dest, &dn);
			if (err)
				goto err;

			/* write dummy data page */
			f2fs_replace_block(sbi, &dn, src, dest,
						ni.version, false, false);
			recovered++;
		}
	}

=======
		if (src != dest && dest != NEW_ADDR && dest != NULL_ADDR) {
			if (src == NULL_ADDR) {
				int err = reserve_new_block(&dn);
				/* We should not get -ENOSPC */
				BUG_ON(err);
			}

			/* Check the previous node page having this index */
			check_index_in_prev_nodes(sbi, dest);

			set_summary(&sum, dn.nid, dn.ofs_in_node, ni.version);

			/* write dummy data page */
			recover_data_page(sbi, NULL, &sum, src, dest);
			update_extent_cache(dest, &dn);
		}
		dn.ofs_in_node++;
	}

	/* write node page in place */
	set_summary(&sum, dn.nid, 0, 0);
>>>>>>> 512ca3c... stock
	if (IS_INODE(dn.node_page))
		sync_inode_page(&dn);

	copy_node_footer(dn.node_page, page);
	fill_node_footer(dn.node_page, dn.nid, ni.ino,
					ofs_of_node(page), false);
	set_page_dirty(dn.node_page);
<<<<<<< HEAD
err:
	f2fs_put_dnode(&dn);
out:
	f2fs_msg(sbi->sb, KERN_NOTICE,
		"recover_data: ino = %lx, recovered = %d blocks, err = %d",
		inode->i_ino, recovered, err);
	return err;
}

static int recover_data(struct f2fs_sb_info *sbi, struct list_head *head)
{
	unsigned long long cp_ver = cur_cp_version(F2FS_CKPT(sbi));
	struct curseg_info *curseg;
	struct page *page = NULL;
=======

	recover_node_page(sbi, dn.node_page, &sum, &ni, blkaddr);
	f2fs_put_dnode(&dn);
	mutex_unlock_op(sbi, ilock);
	return 0;
}

static int recover_data(struct f2fs_sb_info *sbi,
				struct list_head *head, int type)
{
	unsigned long long cp_ver = le64_to_cpu(sbi->ckpt->checkpoint_ver);
	struct curseg_info *curseg;
	struct page *page;
>>>>>>> 512ca3c... stock
	int err = 0;
	block_t blkaddr;

	/* get node pages in the current segment */
<<<<<<< HEAD
	curseg = CURSEG_I(sbi, CURSEG_WARM_NODE);
	blkaddr = NEXT_FREE_BLKADDR(sbi, curseg);

	while (1) {
		struct fsync_inode_entry *entry;

		if (!is_valid_blkaddr(sbi, blkaddr, META_POR))
			break;

		ra_meta_pages_cond(sbi, blkaddr);

		page = get_tmp_page(sbi, blkaddr);

		if (cp_ver != cpver_of_node(page)) {
			f2fs_put_page(page, 1);
			break;
		}
=======
	curseg = CURSEG_I(sbi, type);
	blkaddr = NEXT_FREE_BLKADDR(sbi, curseg);

	/* read node page */
	page = alloc_page(GFP_NOFS | __GFP_ZERO);
	if (IS_ERR(page))
		return -ENOMEM;

	lock_page(page);

	while (1) {
		struct fsync_inode_entry *entry;

		err = f2fs_readpage(sbi, page, blkaddr, READ_SYNC);
		if (err)
			goto out;

		lock_page(page);

		if (cp_ver != cpver_of_node(page))
			goto unlock_out;
>>>>>>> 512ca3c... stock

		entry = get_fsync_inode(head, ino_of_node(page));
		if (!entry)
			goto next;
<<<<<<< HEAD
		/*
		 * inode(x) | CP | inode(x) | dnode(F)
		 * In this case, we can lose the latest inode(x).
		 * So, call recover_inode for the inode update.
		 */
		if (entry->last_inode == blkaddr)
			recover_inode(entry->inode, page);
		if (entry->last_dentry == blkaddr) {
			err = recover_dentry(entry->inode, page);
			if (err) {
				f2fs_put_page(page, 1);
				break;
			}
		}
		err = do_recover_data(sbi, entry->inode, page, blkaddr);
		if (err) {
			f2fs_put_page(page, 1);
			break;
		}
=======

		err = do_recover_data(sbi, entry->inode, page, blkaddr);
		if (err)
			goto out;
>>>>>>> 512ca3c... stock

		if (entry->blkaddr == blkaddr) {
			iput(entry->inode);
			list_del(&entry->list);
			kmem_cache_free(fsync_entry_slab, entry);
		}
next:
		/* check next segment */
		blkaddr = next_blkaddr_of_node(page);
<<<<<<< HEAD
		f2fs_put_page(page, 1);
	}
=======
	}
unlock_out:
	unlock_page(page);
out:
	__free_pages(page, 0);

>>>>>>> 512ca3c... stock
	if (!err)
		allocate_new_segments(sbi);
	return err;
}

int recover_fsync_data(struct f2fs_sb_info *sbi)
{
<<<<<<< HEAD
	struct curseg_info *curseg = CURSEG_I(sbi, CURSEG_WARM_NODE);
	struct list_head inode_list;
	block_t blkaddr;
	int err;
	bool need_writecp = false;

	fsync_entry_slab = f2fs_kmem_cache_create("f2fs_fsync_inode_entry",
			sizeof(struct fsync_inode_entry));
	if (!fsync_entry_slab)
=======
	struct list_head inode_list;
	int err;

	fsync_entry_slab = f2fs_kmem_cache_create("f2fs_fsync_inode_entry",
			sizeof(struct fsync_inode_entry), NULL);
	if (unlikely(!fsync_entry_slab))
>>>>>>> 512ca3c... stock
		return -ENOMEM;

	INIT_LIST_HEAD(&inode_list);

<<<<<<< HEAD
	/* prevent checkpoint */
	mutex_lock(&sbi->cp_mutex);

	blkaddr = NEXT_FREE_BLKADDR(sbi, curseg);

=======
>>>>>>> 512ca3c... stock
	/* step #1: find fsynced inode numbers */
	err = find_fsync_dnodes(sbi, &inode_list);
	if (err)
		goto out;

	if (list_empty(&inode_list))
		goto out;

<<<<<<< HEAD
	need_writecp = true;

	/* step #2: recover data */
	err = recover_data(sbi, &inode_list);
	if (!err)
		f2fs_bug_on(sbi, !list_empty(&inode_list));
out:
	destroy_fsync_dnodes(&inode_list);
	kmem_cache_destroy(fsync_entry_slab);

	/* truncate meta pages to be used by the recovery */
	truncate_inode_pages_range(META_MAPPING(sbi),
			(loff_t)MAIN_BLKADDR(sbi) << PAGE_CACHE_SHIFT, -1);

	if (err) {
		truncate_inode_pages(NODE_MAPPING(sbi), 0);
		truncate_inode_pages(META_MAPPING(sbi), 0);
	}

	clear_sbi_flag(sbi, SBI_POR_DOING);
	if (err) {
		bool invalidate = false;

		if (discard_next_dnode(sbi, blkaddr))
			invalidate = true;

		/* Flush all the NAT/SIT pages */
		while (get_pages(sbi, F2FS_DIRTY_META))
			sync_meta_pages(sbi, META, LONG_MAX);

		/* invalidate temporary meta page */
		if (invalidate)
			invalidate_mapping_pages(META_MAPPING(sbi),
							blkaddr, blkaddr);

		set_ckpt_flags(sbi->ckpt, CP_ERROR_FLAG);
		mutex_unlock(&sbi->cp_mutex);
	} else if (need_writecp) {
		struct cp_control cpc = {
			.reason = CP_RECOVERY,
		};
		mutex_unlock(&sbi->cp_mutex);
		err = write_checkpoint(sbi, &cpc);
	} else {
		mutex_unlock(&sbi->cp_mutex);
	}
=======
	/* step #2: recover data */
	sbi->por_doing = 1;
	err = recover_data(sbi, &inode_list, CURSEG_WARM_NODE);
	sbi->por_doing = 0;
	BUG_ON(!list_empty(&inode_list));
out:
	destroy_fsync_dnodes(sbi, &inode_list);
	kmem_cache_destroy(fsync_entry_slab);
	write_checkpoint(sbi, false);
>>>>>>> 512ca3c... stock
	return err;
}
