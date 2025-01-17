// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Rob Clark <rob.clark@linaro.org>
 */

#include <linux/dma-buf.h>
#include <linux/highmem.h>

#include <drm/drm_prime.h>

#include "omap_drv.h"

/* -----------------------------------------------------------------------------
 * DMABUF Export
 */

static struct sg_table *omap_gem_map_dma_buf(
		struct dma_buf_attachment *attachment,
		enum dma_data_direction dir)
{
	struct drm_gem_object *obj = attachment->dmabuf->priv;
	struct sg_table *sg;

	sg = omap_gem_get_sg(obj);
	if (IS_ERR(sg))
		return sg;

	/* this must be after omap_gem_pin() to ensure we have pages attached */
	omap_gem_dma_sync_buffer(obj, dir);

	return sg;
}

static void omap_gem_unmap_dma_buf(struct dma_buf_attachment *attachment,
		struct sg_table *sg, enum dma_data_direction dir)
{
	struct drm_gem_object *obj = attachment->dmabuf->priv;
	omap_gem_put_sg(obj, sg);
}

static int omap_gem_dmabuf_begin_cpu_access(struct dma_buf *buffer,
		enum dma_data_direction dir)
{
	struct drm_gem_object *obj = buffer->priv;
	struct page **pages;
	if (omap_gem_flags(obj) & OMAP_BO_TILED) {
		/* TODO we would need to pin at least part of the buffer to
		 * get de-tiled view.  For now just reject it.
		 */
		return -ENOMEM;
	}
	/* make sure we have the pages: */
	return omap_gem_get_pages(obj, &pages, true);
}

static int omap_gem_dmabuf_end_cpu_access(struct dma_buf *buffer,
					  enum dma_data_direction dir)
{
	struct drm_gem_object *obj = buffer->priv;
	omap_gem_put_pages(obj);
	return 0;
}

static void *omap_gem_dmabuf_kmap(struct dma_buf *buffer,
		unsigned long page_num)
{
	struct drm_gem_object *obj = buffer->priv;
	struct page **pages;
	omap_gem_get_pages(obj, &pages, false);
	omap_gem_cpu_sync_page(obj, page_num);
	return kmap(pages[page_num]);
}

static void omap_gem_dmabuf_kunmap(struct dma_buf *buffer,
		unsigned long page_num, void *addr)
{
	struct drm_gem_object *obj = buffer->priv;
	struct page **pages;
	omap_gem_get_pages(obj, &pages, false);
	kunmap(pages[page_num]);
}

static int omap_gem_dmabuf_mmap(struct dma_buf *buffer,
		struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = buffer->priv;
	int ret = 0;

	ret = drm_gem_mmap_obj(obj, omap_gem_mmap_size(obj), vma);
	if (ret < 0)
		return ret;

	return omap_gem_mmap_obj(obj, vma);
}

static const struct dma_buf_ops omap_dmabuf_ops = {
	.map_dma_buf = omap_gem_map_dma_buf,
	.unmap_dma_buf = omap_gem_unmap_dma_buf,
	.release = drm_gem_dmabuf_release,
	.begin_cpu_access = omap_gem_dmabuf_begin_cpu_access,
	.end_cpu_access = omap_gem_dmabuf_end_cpu_access,
	.map = omap_gem_dmabuf_kmap,
	.unmap = omap_gem_dmabuf_kunmap,
	.mmap = omap_gem_dmabuf_mmap,
};

struct dma_buf *omap_gem_prime_export(struct drm_gem_object *obj, int flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &omap_dmabuf_ops;
	exp_info.size = obj->size;
	exp_info.flags = flags;
	exp_info.priv = obj;

	return drm_gem_dmabuf_export(obj->dev, &exp_info);
}

/* -----------------------------------------------------------------------------
 * DMABUF Import
 */

struct drm_gem_object *omap_gem_prime_import(struct drm_device *dev,
					     struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct drm_gem_object *obj;
	struct sg_table *sgt;
	int ret;

	if (dma_buf->ops == &omap_dmabuf_ops) {
		obj = dma_buf->priv;
		if (obj->dev == dev) {
			/*
			 * Importing dmabuf exported from out own gem increases
			 * refcount on gem itself instead of f_count of dmabuf.
			 */
			drm_gem_object_get(obj);
			return obj;
		}
	}

	attach = dma_buf_attach(dma_buf, dev->dev);
	if (IS_ERR(attach))
		return ERR_CAST(attach);

	get_dma_buf(dma_buf);

	sgt = dma_buf_map_attachment(attach, DMA_TO_DEVICE);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto fail_detach;
	}

	obj = omap_gem_new_dmabuf(dev, dma_buf->size, sgt);
	if (IS_ERR(obj)) {
		ret = PTR_ERR(obj);
		goto fail_unmap;
	}

	obj->import_attach = attach;

	return obj;

fail_unmap:
	dma_buf_unmap_attachment(attach, sgt, DMA_TO_DEVICE);
fail_detach:
	dma_buf_detach(dma_buf, attach);
	dma_buf_put(dma_buf);

	return ERR_PTR(ret);
}
