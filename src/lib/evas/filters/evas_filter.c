/*
 * \@file evas_filter.c
 *
 * Infrastructure for simple filters applied to RGBA and Alpha buffers.
 * Originally used by font effects.
 *
 * Filters include:
 * - Blur (Gaussian, Box, Motion) and Shadows
 * - Bump maps (light effects)
 * - Displacement maps
 * - Color curves
 * - Blending and masking
 *
 * The reference documentation can be found in evas_filter_parser.c
 */

#include "evas_filter.h"

#ifdef EVAS_CSERVE2
# include "evas_cs2_private.h"
#endif

#include "evas_filter_private.h"
#include <Ector.h>
#include <software/Ector_Software.h>
#include "evas_ector_buffer.eo.h"

#define _assert(a) if (!(a)) CRI("Failed on %s", #a);

static void _buffer_free(Evas_Filter_Buffer *fb);
static void _command_del(Evas_Filter_Context *ctx, Evas_Filter_Command *cmd);
static Evas_Filter_Buffer *_buffer_alloc_new(Evas_Filter_Context *ctx, int w, int h, Eina_Bool alpha_only, Eina_Bool render, Eina_Bool draw);

#define DRAW_COLOR_SET(r, g, b, a) do { cmd->draw.R = r; cmd->draw.G = g; cmd->draw.B = b; cmd->draw.A = a; } while (0)
#define DRAW_CLIP_SET(_x, _y, _w, _h) do { cmd->draw.clip.x = _x; cmd->draw.clip.y = _y; cmd->draw.clip.w = _w; cmd->draw.clip.h = _h; } while (0)
#define DRAW_FILL_SET(fmode) do { cmd->draw.fillmode = fmode; } while (0)

/* Main functions */

Evas_Filter_Context *
evas_filter_context_new(Evas_Public_Data *evas, Eina_Bool async, void *user_data)
{
   Evas_Filter_Context *ctx;
   static int warned = 0;

   EINA_SAFETY_ON_NULL_RETURN_VAL(evas, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(evas->engine.func->gfx_filter_supports, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(evas->engine.func->gfx_filter_process, NULL);

   ctx = calloc(1, sizeof(Evas_Filter_Context));
   if (!ctx) return NULL;

   ctx->evas = evas;
   ctx->async = async;
   ctx->user_data = user_data;
   ctx->buffer_scaled_get = &evas_filter_buffer_scaled_get;

   if (ENFN->gl_surface_read_pixels)
     {
        ctx->gl = EINA_TRUE;
        if (!warned)
          {
             WRN("OpenGL support through SW functions, expect low performance!");
             warned = 1;
          }
     }

   return ctx;
}

void *
evas_filter_context_data_get(Evas_Filter_Context *ctx)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   return ctx->user_data;
}

Eina_Bool
evas_filter_context_async_get(Evas_Filter_Context *ctx)
{
   return ctx->async;
}

/* Private function to reset the filter context. Used from parser.c */
void
evas_filter_context_clear(Evas_Filter_Context *ctx)
{
   Evas_Filter_Buffer *fb;
   Evas_Filter_Command *cmd;

   if (!ctx) return;

   EINA_LIST_FREE(ctx->buffers, fb)
     _buffer_free(fb);
   EINA_INLIST_FREE(ctx->commands, cmd)
     _command_del(ctx, cmd);

   ctx->buffers = NULL;
   ctx->commands = NULL;
   ctx->last_buffer_id = 0;
   ctx->last_command_id = 0;

   // Note: don't reset post_run, as it it set by the client
}

static void
_filter_buffer_backing_free(Evas_Filter_Buffer *fb)
{
   if (!fb || !fb->buffer) return;
   efl_del(fb->buffer);
   fb->buffer = NULL;
}

/** @hidden private render proxy objects */
void
evas_filter_context_proxy_render_all(Evas_Filter_Context *ctx, Eo *eo_obj,
                                     Eina_Bool do_async)
{
   Evas_Object_Protected_Data *source;
   Evas_Object_Protected_Data *obj;
   Evas_Filter_Buffer *fb;
   Eina_List *li;

   if (!ctx->has_proxies) return;
   obj = efl_data_scope_get(eo_obj, EFL_CANVAS_OBJECT_CLASS);

   EINA_LIST_FOREACH(ctx->buffers, li, fb)
     if (fb->source)
       {
          // TODO: Lock current object as proxyrendering (see image obj)
          source = efl_data_scope_get(fb->source, EFL_CANVAS_OBJECT_CLASS);
          _assert(fb->w == source->cur->geometry.w);
          _assert(fb->h == source->cur->geometry.h);
          if (source->proxy->surface && !source->proxy->redraw)
            {
               XDBG("Source already rendered: '%s' of type '%s'",
                   fb->source_name, efl_class_name_get(efl_class_get(fb->source)));
            }
          else
            {
               XDBG("Source needs to be rendered: '%s' of type '%s' (%s)",
                   fb->source_name, efl_class_name_get(efl_class_get(fb->source)),
                   source->proxy->redraw ? "redraw" : "no surface");
               evas_render_proxy_subrender(ctx->evas->evas, fb->source, eo_obj, obj, EINA_FALSE, do_async);
            }
          _filter_buffer_backing_free(fb);
          XDBG("Source #%d '%s' has dimensions %dx%d", fb->id, fb->source_name, fb->w, fb->h);
          fb->buffer = ENFN->ector_buffer_wrap(ENDT, obj->layer->evas->evas, source->proxy->surface);
          fb->alpha_only = EINA_FALSE;
       }
}

void
evas_filter_context_destroy(Evas_Filter_Context *ctx)
{
   Evas_Filter_Buffer *fb;
   Evas_Filter_Command *cmd;

   if (!ctx) return;

   EINA_LIST_FREE(ctx->buffers, fb)
     _buffer_free(fb);
   EINA_INLIST_FREE(ctx->commands, cmd)
     _command_del(ctx, cmd);

   if (ctx->target.mask)
     ctx->evas->engine.func->image_free(ctx->evas->engine.data.output, ctx->target.mask);

   free(ctx);
}

void
evas_filter_context_post_run_callback_set(Evas_Filter_Context *ctx,
                                          Evas_Filter_Cb cb, void *data)
{
   EINA_SAFETY_ON_NULL_RETURN(ctx);
   ctx->post_run.cb = cb;
   ctx->post_run.data = data;
}

static Evas_Filter_Buffer *
_buffer_empty_new(Evas_Filter_Context *ctx, int w, int h, Eina_Bool alpha_only,
                  Eina_Bool transient)
{
   Evas_Filter_Buffer *fb;

   fb = calloc(1, sizeof(Evas_Filter_Buffer));
   if (!fb) return NULL;

   fb->id = ++(ctx->last_buffer_id);
   fb->ctx = ctx;
   fb->alpha_only = alpha_only;
   fb->transient = transient;
   fb->w = w;
   fb->h = h;

   ctx->buffers = eina_list_append(ctx->buffers, fb);
   return fb;
}

static Ector_Buffer *
_ector_buffer_create(Evas_Filter_Buffer const *fb, Eina_Bool render, Eina_Bool draw)
{
   Efl_Gfx_Colorspace cspace = EFL_GFX_COLORSPACE_ARGB8888;
   Ector_Buffer_Flag flags;

   // FIXME: Once all filters are GL buffers need not be CPU accessible
   flags = ECTOR_BUFFER_FLAG_CPU_READABLE | ECTOR_BUFFER_FLAG_CPU_WRITABLE;
   if (render) flags |= ECTOR_BUFFER_FLAG_RENDERABLE;
   if (draw) flags |= ECTOR_BUFFER_FLAG_DRAWABLE;
   if (fb->alpha_only) cspace = EFL_GFX_COLORSPACE_GRY8;

   return fb->ENFN->ector_buffer_new(fb->ENDT, fb->ctx->evas->evas,
                                     fb->w, fb->h, cspace, flags);
}

Eina_Bool
evas_filter_context_buffers_allocate_all(Evas_Filter_Context *ctx)
{
   Evas_Filter_Command *cmd;
   Evas_Filter_Buffer *fb;
   Eina_List *li;
   unsigned w, h;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_FALSE);
   w = ctx->w;
   h = ctx->h;

   XDBG("Allocating all buffers based on output size %ux%u", w, h);

   EINA_INLIST_FOREACH(ctx->commands, cmd)
     {
        Evas_Filter_Fill_Mode fillmode = cmd->draw.fillmode;
        Evas_Filter_Buffer *in, *out;

        in = cmd->input;
        if (!in->w && !in->h)
          {
             in->w = w;
             in->h = h;
          }

        if (fillmode & EVAS_FILTER_FILL_MODE_STRETCH_XY)
          {
             unsigned sw = w, sh = h;

             switch (cmd->mode)
               {
                case EVAS_FILTER_MODE_BLEND:
                  in = cmd->input;
                  break;
                case EVAS_FILTER_MODE_BUMP:
                case EVAS_FILTER_MODE_DISPLACE:
                case EVAS_FILTER_MODE_MASK:
                  in = cmd->mask;
                  break;
                default:
                  CRI("Invalid fillmode set for command %d", cmd->mode);
                  return EINA_FALSE;
               }

             if (in->w) sw = in->w;
             if (in->h) sh = in->h;

             if ((sw != w) || (sh != h))
               {
                  if (fillmode & EVAS_FILTER_FILL_MODE_STRETCH_X)
                    sw = w;
                  if (fillmode & EVAS_FILTER_FILL_MODE_STRETCH_Y)
                    sh = h;

                  fb = _buffer_alloc_new(ctx, sw, sh, in->alpha_only, 1, 1);
                  XDBG("Allocated temporary buffer #%d of size %ux%u %s",
                       fb ? fb->id : -1, sw, sh, in->alpha_only ? "alpha" : "rgba");
                  if (!fb) goto alloc_fail;
                  fb->transient = EINA_TRUE;
               }
          }

        if (cmd->draw.need_temp_buffer)
          {
             unsigned sw = w, sh = h;

             in = cmd->input;
             if (in->w) sw = in->w;
             if (in->h) sh = in->h;

             fb = _buffer_alloc_new(ctx, sw, sh, in->alpha_only, 1, 1);
             XDBG("Allocated temporary buffer #%d of size %ux%u %s",
                  fb ? fb->id : -1, sw, sh, in->alpha_only ? "alpha" : "rgba");
             if (!fb) goto alloc_fail;
             fb->transient = EINA_TRUE;
          }

        out = cmd->output;
        if (!out->w && !out->h)
          {
             out->w = w;
             out->h = h;
          }
     }

   EINA_LIST_FOREACH(ctx->buffers, li, fb)
     {
        Eina_Bool render = EINA_FALSE, draw = EINA_FALSE;

        if (fb->buffer || fb->source)
          continue;

        if (!fb->w && !fb->h)
          {
             ERR("Size of buffer %d should be known at this point. Is this a dangling buffer?", fb->id);
             continue;
          }

        render |= (fb->id == EVAS_FILTER_BUFFER_INPUT_ID);
        render |= fb->is_render;
        draw |= (fb->id == EVAS_FILTER_BUFFER_OUTPUT_ID);

        fb->buffer = _ector_buffer_create(fb, render, draw);
        XDBG("Allocated buffer #%d of size %ux%u %s: %p",
             fb->id, fb->w, fb->h, fb->alpha_only ? "alpha" : "rgba", fb->buffer);
        if (!fb->buffer) goto alloc_fail;
     }

   return EINA_TRUE;

alloc_fail:
   ERR("Buffer allocation failed! Context size: %dx%d", w, h);
   return EINA_FALSE;
}

int
evas_filter_buffer_empty_new(Evas_Filter_Context *ctx, Eina_Bool alpha_only)
{
   Evas_Filter_Buffer *fb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, -1);

   fb = _buffer_empty_new(ctx, 0, 0, alpha_only, EINA_FALSE);
   if (!fb) return -1;

   return fb->id;
}

static Evas_Filter_Buffer *
_buffer_alloc_new(Evas_Filter_Context *ctx, int w, int h, Eina_Bool alpha_only,
                  Eina_Bool render, Eina_Bool draw)
{
   Evas_Filter_Buffer *fb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(w > 0 && h > 0, NULL);

   fb = calloc(1, sizeof(Evas_Filter_Buffer));
   if (!fb) return NULL;

   fb->id = ++(ctx->last_buffer_id);
   fb->ctx = ctx;
   fb->w = w;
   fb->h = h;
   fb->alpha_only = alpha_only;
   fb->buffer = _ector_buffer_create(fb, render, draw);
   if (!fb->buffer)
     {
        ERR("Failed to create ector buffer!");
        free(fb);
        return NULL;
     }

   ctx->buffers = eina_list_append(ctx->buffers, fb);
   return fb;
}

static void
_buffer_free(Evas_Filter_Buffer *fb)
{
   _filter_buffer_backing_free(fb);
   free(fb);
}

Evas_Filter_Buffer *
_filter_buffer_get(Evas_Filter_Context *ctx, int bufid)
{
   Evas_Filter_Buffer *buffer;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   EINA_LIST_FOREACH(ctx->buffers, l, buffer)
     if (buffer->id == bufid) return buffer;

   return NULL;
}

void *
evas_filter_buffer_backing_steal(Evas_Filter_Context *ctx, int bufid)
{
   Evas_Filter_Buffer *fb;

   fb = _filter_buffer_get(ctx, bufid);
   if (!fb) return NULL;

   return evas_ector_buffer_drawable_image_get(fb->buffer);
}

Eina_Bool
evas_filter_buffer_backing_release(Evas_Filter_Context *ctx,
                                   void *stolen_buffer)
{
   if (!stolen_buffer) return EINA_FALSE;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_FALSE);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(eina_main_loop_is(), EINA_FALSE);

   ENFN->image_free(ENDT, stolen_buffer);
   return EINA_TRUE;
}

static Evas_Filter_Command *
_command_new(Evas_Filter_Context *ctx, Evas_Filter_Mode mode,
             Evas_Filter_Buffer *input, Evas_Filter_Buffer *mask,
             Evas_Filter_Buffer *output)
{
   Evas_Filter_Command *cmd;

   cmd = calloc(1, sizeof(Evas_Filter_Command));
   if (!cmd) return NULL;

   cmd->id = ++(ctx->last_command_id);
   cmd->ctx = ctx;
   cmd->mode = mode;
   cmd->input = input;
   cmd->mask = mask;
   cmd->output = output;
   cmd->draw.R = 255;
   cmd->draw.G = 255;
   cmd->draw.B = 255;
   cmd->draw.A = 255;
   if (output) output->dirty = EINA_TRUE;

   ctx->commands = eina_inlist_append(ctx->commands, EINA_INLIST_GET(cmd));
   return cmd;
}

static void
_command_del(Evas_Filter_Context *ctx, Evas_Filter_Command *cmd)
{
   if (!ctx || !cmd) return;
   ctx->commands = eina_inlist_remove(ctx->commands, EINA_INLIST_GET(cmd));
   switch (cmd->mode)
     {
      case EVAS_FILTER_MODE_CURVE: free(cmd->curve.data); break;
      default: break;
     }
   free(cmd);
}

Evas_Filter_Buffer *
evas_filter_temporary_buffer_get(Evas_Filter_Context *ctx, int w, int h,
                                 Eina_Bool alpha_only, Eina_Bool clean)
{
   Evas_Filter_Buffer *fb = NULL;
   Eina_List *l;

   EINA_LIST_FOREACH(ctx->buffers, l, fb)
     {
        if (fb->transient && !fb->locked && (fb->alpha_only == alpha_only)
            && (!clean || !fb->dirty))
          {
             if ((!w || (w == fb->w)) && (!h || (h == fb->h)))
               {
                  fb->locked = EINA_TRUE;
                  return fb;
               }
          }
     }

   if (ctx->running)
     {
        ERR("Can not create a new buffer while filter is running!");
        return NULL;
     }

   fb = _buffer_empty_new(ctx, w, h, alpha_only, EINA_TRUE);
   fb->locked = EINA_TRUE;
   XDBG("Created temporary buffer %d %s", fb->id, alpha_only ? "alpha" : "rgba");

   return fb;
}

static void
_filter_buffer_unlock_all(Evas_Filter_Context *ctx)
{
   Evas_Filter_Buffer *buf = NULL;
   Eina_List *l;

   EINA_LIST_FOREACH(ctx->buffers, l, buf)
     buf->locked = EINA_FALSE;
}

Evas_Filter_Command *
evas_filter_command_fill_add(Evas_Filter_Context *ctx, void *draw_context,
                             int bufid)
{
   Evas_Filter_Command *cmd;
   Evas_Filter_Buffer *buf = NULL;
   int R, G, B, A, cx, cy, cw, ch;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(draw_context, NULL);

   buf = _filter_buffer_get(ctx, bufid);
   if (!buf)
     {
        ERR("Buffer %d does not exist.", bufid);
        return NULL;
     }

   cmd = _command_new(ctx, EVAS_FILTER_MODE_FILL, buf, NULL, buf);
   if (!cmd) return NULL;

   ENFN->context_color_get(ENDT, draw_context, &R, &G, &B, &A);
   DRAW_COLOR_SET(R, G, B, A);

   ENFN->context_clip_get(ENDT, draw_context, &cx, &cy, &cw, &ch);
   DRAW_CLIP_SET(cx, cy, cw, ch);

   XDBG("Add fill %d with color(%d,%d,%d,%d)", buf->id, R, G, B, A);

   if (!R && !G && !B && !A)
     buf->dirty = EINA_FALSE;

   return cmd;
}

Evas_Filter_Command *
evas_filter_command_blur_add(Evas_Filter_Context *ctx, void *drawctx,
                             int inbuf, int outbuf, Evas_Filter_Blur_Type type,
                             int dx, int dy, int ox, int oy, int count)
{
   Evas_Filter_Buffer *in = NULL, *out = NULL, *tmp = NULL, *in_dy = NULL;
   Evas_Filter_Buffer *out_dy = NULL, *out_dx = NULL;
   Evas_Filter_Buffer *copybuf = NULL, *blendbuf = NULL;
   Evas_Filter_Command *cmd = NULL;
   int R, G, B, A, render_op;
   Eina_Bool override;
   DATA32 color;

   // Note (SW engine):
   // The basic blur operation overrides the pixels in the target buffer,
   // only supports one direction (X or Y) and no offset. As a consequence
   // most cases require intermediate work buffers.

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(drawctx, NULL);

   if (dx < 0) dx = 0;
   if (dy < 0) dy = 0;
   if (!dx && !dy)
     {
        XDBG("Changing 0px blur into simple blend");
        return evas_filter_command_blend_add(ctx, drawctx, inbuf, outbuf, ox, oy, EVAS_FILTER_FILL_MODE_NONE);
     }

   in = _filter_buffer_get(ctx, inbuf);
   EINA_SAFETY_ON_FALSE_GOTO(in, fail);

   out = _filter_buffer_get(ctx, outbuf);
   EINA_SAFETY_ON_FALSE_GOTO(out, fail);

   if (in == out) out->dirty = EINA_FALSE;

   ENFN->context_color_get(ENDT, drawctx, &R, &G, &B, &A);
   color = ARGB_JOIN(A, R, G, B);
   if (!color)
     {
        DBG("Blur with transparent color. Nothing to do.");
        return _command_new(ctx, EVAS_FILTER_MODE_SKIP, NULL, NULL, NULL);
     }

   render_op = ENFN->context_render_op_get(ENDT, drawctx);
   override = (render_op == EVAS_RENDER_COPY);

   switch (type)
     {
      case EVAS_FILTER_BLUR_GAUSSIAN:
        count = 1;
        break;

      case EVAS_FILTER_BLUR_BOX:
        count = MIN(MAX(1, count), 6);
        break;

      case EVAS_FILTER_BLUR_DEFAULT:
        {
           /* In DEFAULT mode we cheat, depending on the size of the kernel:
            * For 1px to 2px, use true Gaussian blur.
            * For 3px to 6px, use two Box blurs.
            * For more than 6px, use three Box blurs.
            * This will give both nicer and MUCH faster results than Gaussian.
            *
            * NOTE: This step should be avoided in GL.
            */

           int tmp_out = outbuf;
           int tmp_in = inbuf;
           int tmp_ox = ox;
           int tmp_oy = oy;

           // For 2D blur: create intermediate buffer
           if (dx && dy)
             {
                tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 1);
                if (!tmp) goto fail;
                tmp_in = tmp_out = tmp->id;
                tmp_ox = tmp_oy = 0;
             }

           // X box blur
           if (dx)
             {
                if (dx <= 2)
                  type = EVAS_FILTER_BLUR_GAUSSIAN;
                else
                  type = EVAS_FILTER_BLUR_BOX;

                if (dy) ENFN->context_color_set(ENDT, drawctx, 255, 255, 255, 255);
                cmd = evas_filter_command_blur_add(ctx, drawctx, inbuf, tmp_out,
                                                   type, dx, 0, tmp_ox, tmp_oy, 0);
                if (!cmd) goto fail;
                cmd->blur.auto_count = EINA_TRUE;
                if (dy) ENFN->context_color_set(ENDT, drawctx, R, G, B, A);
             }

           // Y box blur
           if (dy)
             {
                if (dy <= 2)
                  type = EVAS_FILTER_BLUR_GAUSSIAN;
                else
                  type = EVAS_FILTER_BLUR_BOX;

                if (dx && (inbuf == outbuf))
                  ENFN->context_render_op_set(ENDT, drawctx, EVAS_RENDER_COPY);
                cmd = evas_filter_command_blur_add(ctx, drawctx, tmp_in, outbuf,
                                                   type, 0, dy, ox, oy, 0);
                if (dx && (inbuf == outbuf))
                  ENFN->context_render_op_set(ENDT, drawctx, render_op);
                if (!cmd) goto fail;
                cmd->blur.auto_count = EINA_TRUE;
             }

           return cmd;
        }

      default:
        CRI("Not implemented yet!");
        goto fail;
     }

   // For 2D blur: create intermediate buffer between X and Y passes
   if (dx && dy)
     {
        // If there's an offset: create intermediate buffer before offset blend
        if (ox || oy)
          {
             copybuf = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
             if (!copybuf) goto fail;
          }

        // Intermediate buffer between X and Y passes
        tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
        if (!tmp) goto fail;

        if (in == out)
          {
             // IN = OUT and 2-D blur. IN -blur-> TMP -blur-> IN.
             out_dx = tmp;
             in_dy = tmp;
             out_dy = copybuf ? copybuf : in;
          }
        else
          {
             // IN != OUT and 2-D blur. IN -blur-> TMP -blur-> OUT.
             out_dx = tmp;
             in_dy = tmp;
             out_dy = copybuf ? copybuf : out;
          }
     }
   else if (dx)
     {
        // X blur only
        if (in == out)
          {
             // IN = OUT and 1-D blur. IN -blur-> TMP -copy-> IN.
             tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
             if (!tmp) goto fail;
             copybuf = tmp;
             out_dx = tmp;
          }
        else if (ox || oy || (color != 0xFFFFFFFF))
          {
             // IN != OUT and 1-D blur. IN -blur-> TMP -blend-> OUT.
             tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
             if (!tmp) goto fail;
             blendbuf = tmp;
             out_dx = tmp;
          }
        else if (out->dirty)
          {
             // IN != OUT and 1-D blur. IN -blur-> TMP -blend-> OUT.
             tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
             if (!tmp) goto fail;
             blendbuf = tmp;
             out_dx = tmp;
          }
        else
          {
             // IN != OUT and 1-D blur. IN -blur-> OUT.
             out_dx = out;
          }
     }
   else
     {
        // Y blur only
        if (in == out)
          {
             // IN = OUT and 1-D blur. IN -blur-> TMP -copy-> IN.
             tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
             if (!tmp) goto fail;
             copybuf = tmp;
             in_dy = in;
             out_dy = tmp;
          }
        else if (ox || oy || (color != 0xFFFFFFFF))
          {
             // IN != OUT and 1-D blur. IN -blur-> TMP -blend-> IN.
             tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
             if (!tmp) goto fail;
             if (override)
               copybuf = tmp;
             else
               blendbuf = tmp;
             in_dy = in;
             out_dy = tmp;
          }
        else if (out->dirty && !override)
          {
             // IN != OUT and 1-D blur. IN -blur-> TMP -blend-> OUT.
             tmp = evas_filter_temporary_buffer_get(ctx, 0, 0, in->alpha_only, 0);
             if (!tmp) goto fail;
             blendbuf = tmp;
             in_dy = in;
             out_dy = tmp;
          }
        else
          {
             // IN != OUT and 1-D blur. IN -blur-> OUT.
             in_dy = in;
             out_dy = out;
          }
     }

   if (dx)
     {
        XDBG("Add horizontal blur %d -> %d (%dpx)", in->id, out_dx->id, dx);
        cmd = _command_new(ctx, EVAS_FILTER_MODE_BLUR, in, NULL, out_dx);
        if (!cmd) goto fail;
        cmd->blur.type = type;
        cmd->blur.dx = dx;
        cmd->blur.dy = 0;
        cmd->blur.count = count;
        if (!dy) DRAW_COLOR_SET(R, G, B, A);
     }

   if (dy)
     {
        XDBG("Add vertical blur %d -> %d (%dpx)", in_dy->id, out_dy->id, dy);
        cmd = _command_new(ctx, EVAS_FILTER_MODE_BLUR, in_dy, NULL, out_dy);
        if (!cmd) goto fail;
        cmd->blur.type = type;
        cmd->blur.dx = 0;
        cmd->blur.dy = dy;
        cmd->blur.count = count;
        DRAW_COLOR_SET(R, G, B, A);
     }

   if (blendbuf)
     {
        Evas_Filter_Command *blendcmd;

        XDBG("Add extra blend %d -> %d", blendbuf->id, out->id);
        blendcmd = evas_filter_command_blend_add(ctx, drawctx,
                                                 blendbuf->id, out->id, ox, oy,
                                                 EVAS_FILTER_FILL_MODE_NONE);
        if (!blendcmd) goto fail;
        ox = oy = 0;
     }
   else if (copybuf)
     {
        Evas_Filter_Command *copycmd;

        XDBG("Add extra copy %d -> %d: offset: %d,%d", copybuf->id, out->id, ox, oy);
        ENFN->context_color_set(ENDT, drawctx, 255, 255, 255, 255);
        ENFN->context_render_op_set(ENDT, drawctx, EVAS_RENDER_COPY);
        copycmd = evas_filter_command_blend_add(ctx, drawctx,
                                                copybuf->id, out->id, ox, oy,
                                                EVAS_FILTER_FILL_MODE_NONE);
        ENFN->context_color_set(ENDT, drawctx, R, G, B, A);
        ENFN->context_render_op_set(ENDT, drawctx, render_op);
        if (!copycmd) goto fail;
        ox = oy = 0;
     }

   out->dirty = EINA_TRUE;
   _filter_buffer_unlock_all(ctx);
   return cmd;

fail:
   ERR("Failed to add blur");
   _filter_buffer_unlock_all(ctx);
   return NULL;
}

Evas_Filter_Command *
evas_filter_command_blend_add(Evas_Filter_Context *ctx, void *drawctx,
                              int inbuf, int outbuf, int ox, int oy,
                              Evas_Filter_Fill_Mode fillmode)
{
   Evas_Filter_Command *cmd;
   Evas_Filter_Buffer *in, *out;
   Eina_Bool copy;
   int R, G, B, A;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   if (inbuf == outbuf)
     {
        XDBG("Skipping NOP blend operation %d --> %d", inbuf, outbuf);
        return NULL;
     }

   in = _filter_buffer_get(ctx, inbuf);
   if (!in)
     {
        ERR("Buffer %d does not exist [input].", inbuf);
        return NULL;
     }

   out = _filter_buffer_get(ctx, outbuf);
   if (!out)
     {
        ERR("Buffer %d does not exist [output].", outbuf);
        return NULL;
     }

   cmd = _command_new(ctx, EVAS_FILTER_MODE_BLEND, in, NULL, out);
   if (!cmd) return NULL;

   if (ENFN->context_render_op_get(ENDT, drawctx) == EVAS_RENDER_COPY)
     copy = EINA_TRUE;
   else
     copy = EINA_FALSE;

   ENFN->context_color_get(ENDT, drawctx, &R, &G, &B, &A);
   DRAW_COLOR_SET(R, G, B, A);
   DRAW_FILL_SET(fillmode);
   cmd->draw.ox = ox;
   cmd->draw.oy = oy;
   cmd->draw.rop = copy ? EFL_GFX_RENDER_OP_COPY : EFL_GFX_RENDER_OP_BLEND;
   cmd->draw.clip_use =
         ENFN->context_clip_get(ENDT, drawctx,
                                &cmd->draw.clip.x, &cmd->draw.clip.y,
                                &cmd->draw.clip.w, &cmd->draw.clip.h);

   XDBG("Add %s %d -> %d: offset %d,%d, color: %d,%d,%d,%d",
        copy ? "copy" : "blend", in->id, out->id, ox, oy, R, G, B, A);
   if (cmd->draw.clip_use)
     XDBG("Draw clip: %d,%d,%d,%d", cmd->draw.clip.x, cmd->draw.clip.y,
         cmd->draw.clip.w, cmd->draw.clip.h);

   out->dirty = EINA_TRUE;
   return cmd;
}

Evas_Filter_Command *
evas_filter_command_grow_add(Evas_Filter_Context *ctx, void *draw_context,
                             int inbuf, int outbuf, int radius, Eina_Bool smooth)
{
   Evas_Filter_Command *blurcmd, *threshcmd, *blendcmd;
   Evas_Filter_Buffer *tmp = NULL, *in, *out;
   int diam = abs(radius) * 2 + 1;
   DATA8 curve[256] = {0};
   int tmin = 0, growbuf;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   if (!radius)
     {
        XDBG("Changing 0px grow into simple blend");
        return evas_filter_command_blend_add(ctx, draw_context, inbuf, outbuf, 0, 0, EVAS_FILTER_FILL_MODE_NONE);
     }

   in = _filter_buffer_get(ctx, inbuf);
   EINA_SAFETY_ON_NULL_RETURN_VAL(in, NULL);

   out = _filter_buffer_get(ctx, outbuf);
   EINA_SAFETY_ON_NULL_RETURN_VAL(out, NULL);

   if ((inbuf != outbuf) && out->dirty)
     {
        tmp = evas_filter_temporary_buffer_get(ctx, in->w, in->h, in->alpha_only, 1);
        EINA_SAFETY_ON_NULL_RETURN_VAL(tmp, NULL);
        growbuf = tmp->id;
     }
   else
     growbuf = outbuf;

   blurcmd = evas_filter_command_blur_add(ctx, draw_context, inbuf, growbuf,
                                          EVAS_FILTER_BLUR_DEFAULT,
                                          abs(radius), abs(radius), 0, 0, 0);
   if (!blurcmd) return NULL;

   if (diam > 255) diam = 255;
   if (radius > 0)
     tmin = 255 / diam;
   else if (radius < 0)
     tmin = 256 - (255 / diam);

   if (!smooth)
     memset(curve + tmin, 255, 256 - tmin);
   else
     {
        int k, start, end, range;

        // This is pretty experimental.
        range = MAX(2, 12 - radius);
        start = ((tmin > range) ? (tmin - range) : 0);
        end = ((tmin < (256 - range)) ? (tmin + range) : 256);

        for (k = start; k < end; k++)
          curve[k] = ((k - start) * 255) / (end - start);
        if (end < 256)
          memset(curve + end, 255, 256 - end);
     }

   threshcmd = evas_filter_command_curve_add(ctx, draw_context, growbuf, growbuf,
                                             curve, EVAS_FILTER_CHANNEL_ALPHA);
   if (!threshcmd)
     {
        _command_del(ctx, blurcmd);
        return NULL;
     }

   if (tmp)
     {
        blendcmd = evas_filter_command_blend_add(ctx, draw_context, tmp->id,
                                                 outbuf, 0, 0,
                                                 EVAS_FILTER_FILL_MODE_NONE);
        if (!blendcmd)
          {
             _command_del(ctx, threshcmd);
             _command_del(ctx, blurcmd);
             return NULL;
          }
     }

   return blurcmd;
}

Evas_Filter_Command *
evas_filter_command_curve_add(Evas_Filter_Context *ctx,
                              void *draw_context EINA_UNUSED,
                              int inbuf, int outbuf, DATA8 *curve,
                              Evas_Filter_Channel channel)
{
   Evas_Filter_Command *cmd;
   Evas_Filter_Buffer *in, *out;
   DATA8 *copy;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(curve, NULL);

   in = _filter_buffer_get(ctx, inbuf);
   out = _filter_buffer_get(ctx, outbuf);
   EINA_SAFETY_ON_NULL_RETURN_VAL(in, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(out, NULL);

   if (in->alpha_only != out->alpha_only)
     WRN("Incompatible formats for color curves, implicit conversion will be "
         "slow and may not produce the desired output.");

   XDBG("Add curve %d -> %d", in->id, out->id);

   copy = malloc(256 * sizeof(DATA8));
   if (!copy) return NULL;

   cmd = _command_new(ctx, EVAS_FILTER_MODE_CURVE, in, NULL, out);
   if (!cmd)
     {
        free(copy);
        return NULL;
     }

   memcpy(copy, curve, 256 * sizeof(DATA8));
   cmd->curve.data = copy;
   cmd->curve.channel = channel;

   return cmd;
}

Evas_Filter_Command *
evas_filter_command_displacement_map_add(Evas_Filter_Context *ctx,
                                         void *draw_context EINA_UNUSED,
                                         int inbuf, int outbuf, int dispbuf,
                                         Evas_Filter_Displacement_Flags flags,
                                         int intensity,
                                         Evas_Filter_Fill_Mode fillmode)
{
   Evas_Filter_Buffer *in, *out, *map, *tmp = NULL, *disp_out;
   Evas_Filter_Command *cmd = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(intensity >= 0, NULL);

   in = _filter_buffer_get(ctx, inbuf);
   out = _filter_buffer_get(ctx, outbuf);
   map = _filter_buffer_get(ctx, dispbuf);
   EINA_SAFETY_ON_NULL_RETURN_VAL(in, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(out, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(map, NULL);

   if (in->alpha_only != out->alpha_only)
     DBG("Different color formats, implicit conversion may be slow");

   if (map->alpha_only)
     {
        WRN("Displacement map is not an RGBA buffer, X and Y axes will be "
            "displaced together.");
     }

   if (in == out)
     {
        tmp = evas_filter_temporary_buffer_get(ctx, in->w, in->h, in->alpha_only, 1);
        if (!tmp) return NULL;
        disp_out = tmp;
     }
   else disp_out = out;

   cmd = _command_new(ctx, EVAS_FILTER_MODE_DISPLACE, in, map, disp_out);
   if (!cmd) goto fail;

   DRAW_FILL_SET(fillmode);
   cmd->displacement.flags = flags & EVAS_FILTER_DISPLACE_BITMASK;
   cmd->displacement.intensity = intensity;
   cmd->draw.rop = _evas_to_gfx_render_op(ENFN->context_render_op_get(ENDT, draw_context));

   if (tmp)
     {
        Evas_Filter_Command *fillcmd;

        fillcmd = evas_filter_command_blend_add(ctx, draw_context, disp_out->id,
                                                out->id, 0, 0,
                                                EVAS_FILTER_FILL_MODE_NONE);
        if (!fillcmd) goto fail;
     }

   _filter_buffer_unlock_all(ctx);
   return cmd;

fail:
   _filter_buffer_unlock_all(ctx);
   _command_del(ctx, cmd);
   return NULL;
}

Evas_Filter_Command *
evas_filter_command_mask_add(Evas_Filter_Context *ctx, void *draw_context,
                             int inbuf, int maskbuf, int outbuf,
                             Evas_Filter_Fill_Mode fillmode)
{
   Evas_Filter_Command *cmd;
   Evas_Filter_Buffer *in, *out, *mask;
   Efl_Gfx_Render_Op render_op;
   int R, G, B, A;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   render_op = _evas_to_gfx_render_op(ENFN->context_render_op_get(ENDT, draw_context));
   ENFN->context_color_get(ENDT, draw_context, &R, &G, &B, &A);

   in = _filter_buffer_get(ctx, inbuf);
   out = _filter_buffer_get(ctx, outbuf);
   mask = _filter_buffer_get(ctx, maskbuf);
   EINA_SAFETY_ON_NULL_RETURN_VAL(in, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(out, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(mask, NULL);

   cmd = _command_new(ctx, EVAS_FILTER_MODE_MASK, in, mask, out);
   if (!cmd) return NULL;

   cmd->draw.rop = render_op;
   DRAW_COLOR_SET(R, G, B, A);
   DRAW_FILL_SET(fillmode);

   return cmd;
}

Evas_Filter_Command *
evas_filter_command_bump_map_add(Evas_Filter_Context *ctx,
                                 void *draw_context EINA_UNUSED,
                                 int inbuf, int bumpbuf, int outbuf,
                                 float xyangle, float zangle, float elevation,
                                 float sf,
                                 DATA32 black, DATA32 color, DATA32 white,
                                 Evas_Filter_Bump_Flags flags,
                                 Evas_Filter_Fill_Mode fillmode)
{
   Evas_Filter_Command *cmd;
   Evas_Filter_Buffer *in, *out, *map;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   in = _filter_buffer_get(ctx, inbuf);
   out = _filter_buffer_get(ctx, outbuf);
   map = _filter_buffer_get(ctx, bumpbuf);
   EINA_SAFETY_ON_NULL_RETURN_VAL(in, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(out, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(map, NULL);

   if (!map->alpha_only)
     DBG("Bump map is not an Alpha buffer, implicit conversion may be slow");

   // FIXME: Boo!
   if (!in->alpha_only)
     WRN("RGBA bump map support is not implemented! This will trigger conversion.");

   // FIXME: Must ensure in != out
   EINA_SAFETY_ON_FALSE_RETURN_VAL(in != out, NULL);
   EINA_SAFETY_ON_FALSE_RETURN_VAL(map != out, NULL);

   cmd = _command_new(ctx, EVAS_FILTER_MODE_BUMP, in, map, out);
   if (!cmd) return NULL;

   DRAW_FILL_SET(fillmode);
   cmd->bump.xyangle = xyangle;
   cmd->bump.zangle = zangle;
   cmd->bump.specular_factor = sf;
   cmd->bump.dark = black;
   cmd->bump.color = color;
   cmd->bump.white = white;
   cmd->bump.elevation = elevation;
   cmd->bump.compensate = !!(flags & EVAS_FILTER_BUMP_COMPENSATE);

   return cmd;
}

Evas_Filter_Command *
evas_filter_command_transform_add(Evas_Filter_Context *ctx,
                                  void *draw_context EINA_UNUSED,
                                  int inbuf, int outbuf,
                                  Evas_Filter_Transform_Flags flags,
                                  int ox, int oy)
{
   Evas_Filter_Command *cmd;
   Evas_Filter_Buffer *in, *out;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);

   in = _filter_buffer_get(ctx, inbuf);
   out = _filter_buffer_get(ctx, outbuf);
   EINA_SAFETY_ON_NULL_RETURN_VAL(in, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(out, NULL);

   cmd = _command_new(ctx, EVAS_FILTER_MODE_TRANSFORM, in, NULL, out);
   if (!cmd) return NULL;

   DRAW_COLOR_SET(255, 255, 255, 255);
   cmd->transform.flags = flags;
   cmd->draw.ox = ox;
   cmd->draw.oy = oy;

   if (in->alpha_only == out->alpha_only)
     {
        DBG("Incompatible buffer formats, will trigger implicit conversion.");
        cmd->draw.rop = EFL_GFX_RENDER_OP_COPY;
     }
   else
     cmd->draw.rop = EFL_GFX_RENDER_OP_BLEND;

   return cmd;
}

/* Final target */
Eina_Bool
evas_filter_target_set(Evas_Filter_Context *ctx, void *draw_context,
                       void *surface, int x, int y)
{
   void *mask = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_FALSE);

   ctx->target.surface = ENFN->image_ref(ENDT, surface);
   ctx->target.x = x;
   ctx->target.y = y;
   ctx->target.clip_use = ENFN->context_clip_get
         (ENDT, draw_context, &ctx->target.cx, &ctx->target.cy,
          &ctx->target.cw, &ctx->target.ch);
   ctx->target.color_use = ENFN->context_multiplier_get
         (ENDT, draw_context, &ctx->target.r, &ctx->target.g,
          &ctx->target.b, &ctx->target.a);
   if (ctx->target.r == 255 && ctx->target.g == 255 &&
       ctx->target.b == 255 && ctx->target.a == 255)
     ctx->target.color_use = EINA_FALSE;
   ctx->target.rop = ENFN->context_render_op_get(ENDT, draw_context);

   ENFN->context_clip_image_get
      (ENDT, draw_context, &mask, &ctx->target.mask_x, &ctx->target.mask_y);
   if (ctx->target.mask)
     ctx->evas->engine.func->image_free(ctx->evas->engine.data.output, ctx->target.mask);
   ctx->target.mask = mask;

   return EINA_TRUE;
}

static Eina_Bool
_filter_target_render(Evas_Filter_Context *ctx)
{
   Evas_Filter_Buffer *src;
   void *drawctx, *image = NULL, *surface;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx->target.surface, EINA_FALSE);

   drawctx = ENFN->context_new(ENDT);
   surface = ctx->target.surface;

   src = _filter_buffer_get(ctx, EVAS_FILTER_BUFFER_OUTPUT_ID);
   EINA_SAFETY_ON_NULL_RETURN_VAL(src, EINA_FALSE);

   image = evas_ector_buffer_drawable_image_get(src->buffer);
   EINA_SAFETY_ON_NULL_GOTO(image, fail);

   // FIXME: Use ector buffer RENDERER here

   if (ctx->target.clip_use)
     {
        ENFN->context_clip_set(ENDT, drawctx, ctx->target.cx, ctx->target.cy,
                               ctx->target.cw, ctx->target.ch);
     }

   if (ctx->target.color_use)
     {
        ENFN->context_multiplier_set(ENDT, drawctx,
                                     ctx->target.r, ctx->target.g,
                                     ctx->target.b, ctx->target.a);
     }

   if (ctx->target.mask)
     {
        ENFN->context_clip_image_set(ENDT, drawctx, ctx->target.mask,
                                     ctx->target.mask_x, ctx->target.mask_y,
                                     ctx->evas, EINA_FALSE);
     }

   ENFN->context_render_op_set(ENDT, drawctx, ctx->target.rop);
   ENFN->image_draw(ENDT, drawctx, surface, image,
                    0, 0, src->w, src->h,
                    ctx->target.x, ctx->target.y, src->w, src->h,
                    EINA_TRUE, EINA_FALSE);

   ENFN->context_free(ENDT, drawctx);
   evas_ector_buffer_engine_image_release(src->buffer, image);

   ENFN->image_free(ENDT, surface);
   ctx->target.surface = NULL;

   return EINA_TRUE;

fail:
   ENFN->image_free(ENDT, surface);
   ctx->target.surface = NULL;

   ERR("Failed to render filter to target canvas!");
   return EINA_FALSE;
}


/* Font drawing stuff */
Eina_Bool
evas_filter_font_draw(Evas_Filter_Context *ctx, void *draw_context, int bufid,
                      Evas_Font_Set *font, int x, int y,
                      Evas_Text_Props *text_props, Eina_Bool do_async)
{
   Eina_Bool async_unref;
   Evas_Filter_Buffer *fb;
   void *surface = NULL;

   fb = _filter_buffer_get(ctx, bufid);
   EINA_SAFETY_ON_NULL_RETURN_VAL(fb, EINA_FALSE);

   surface = evas_ector_buffer_render_image_get(fb->buffer);
   EINA_SAFETY_ON_NULL_RETURN_VAL(surface, EINA_FALSE);

   // Copied from evas_font_draw_async_check
   async_unref = ENFN->font_draw(ENDT, draw_context, surface,
                                 font, x, y, fb->w, fb->h, fb->w, fb->h,
                                 text_props, do_async);
   if (do_async && async_unref)
     {
        evas_common_font_glyphs_ref(text_props->glyphs);
        evas_unref_queue_glyph_put(ctx->evas, text_props->glyphs);
     }

   evas_ector_buffer_engine_image_release(fb->buffer, surface);
   return EINA_TRUE;
}


/* Image draw: scale and draw an original image into a RW surface */
Eina_Bool
evas_filter_image_draw(Evas_Filter_Context *ctx, void *draw_context, int bufid,
                       void *image, Eina_Bool do_async)
{
   int dw = 0, dh = 0, w = 0, h = 0;
   Eina_Bool async_unref;
   Evas_Filter_Buffer *fb;
   void *surface;

   ENFN->image_size_get(ENDT, image, &w, &h);
   if (!w || !h) return EINA_FALSE;

   fb = _filter_buffer_get(ctx, bufid);
   if (!fb) return EINA_FALSE;

   surface = evas_ector_buffer_render_image_get(fb->buffer);
   EINA_SAFETY_ON_NULL_RETURN_VAL(surface, EINA_FALSE);

   ENFN->image_size_get(ENDT, image, &dw, &dh);
   if (!dw || !dh) return EINA_FALSE;

   async_unref = ENFN->image_draw(ENDT, draw_context, surface, image,
                                  0, 0, w, h,
                                  0, 0, dw, dh,
                                  EINA_TRUE, do_async);
   if (do_async && async_unref)
     {
        ENFN->image_ref(ENDT, image);
        evas_unref_queue_image_put(ctx->evas, image);
     }

   evas_ector_buffer_engine_image_release(fb->buffer, surface);
   return EINA_TRUE;
}


/* Clip full input rect (0, 0, sw, sh) to target (dx, dy, dw, dh)
 * and get source's clipped sx, sy as well as destination x, y, cols and rows */
void
_clip_to_target(int *sx /* OUT */, int *sy /* OUT */, int sw, int sh,
                int ox, int oy, int dw, int dh,
                int *dx /* OUT */, int *dy /* OUT */,
                int *rows /* OUT */, int *cols /* OUT */)
{
   if (ox > 0)
     {
        (*sx) = 0;
        (*dx) = ox;
        (*cols) = sw;
        if (((*dx) + (*cols)) > (dw))
          (*cols) = dw - (*dx);
     }
   else if (ox < 0)
     {
        (*dx) = 0;
        (*sx) = (-ox);
        (*cols) = sw - (*sx);
        if ((*cols) > dw) (*cols) = dw;
     }
   else
     {
        (*sx) = 0;
        (*dx) = 0;
        (*cols) = sw;
        if ((*cols) > dw) (*cols) = dw;
     }

   if (oy > 0)
     {
        (*sy) = 0;
        (*dy) = oy;
        (*rows) = sh;
        if (((*dy) + (*rows)) > (dh))
          (*rows) = dh - (*dy);
     }
   else if (oy < 0)
     {
        (*dy) = 0;
        (*sy) = (-oy);
        (*rows) = sh - (*sy);
        if ((*rows) > dh) (*rows) = dh;
     }
   else
     {
        (*sy) = 0;
        (*dy) = 0;
        (*rows) = sh;
        if ((*rows) > dh) (*rows) = dh;
     }
   if ((*cols) < 0) *cols = 0;
   if ((*rows) < 0) *rows = 0;
}

#ifdef FILTERS_DEBUG
static const char *
_filter_name_get(int mode)
{
#define FNAME(a) case EVAS_FILTER_MODE_ ## a: return "EVAS_FILTER_MODE_" #a
   switch (mode)
     {
      FNAME(SKIP);
      FNAME(BLEND);
      FNAME(BLUR);
      FNAME(CURVE);
      FNAME(DISPLACE);
      FNAME(MASK);
      FNAME(BUMP);
      FNAME(FILL);
      default: return "INVALID";
     }
#undef FNAME
}
#endif

static Eina_Bool
_filter_command_run(Evas_Filter_Command *cmd)
{
   Evas_Filter_Support support = EVAS_FILTER_SUPPORT_NONE;

   if (cmd->mode == EVAS_FILTER_MODE_SKIP)
     return EINA_TRUE;

   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd->output, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cmd->input, EINA_FALSE);

#ifdef FILTERS_DEBUG
   XDBG("Command %d (%s): %d [%d] --> %d",
       cmd->id, _filter_name_get(cmd->mode),
       cmd->input->id, cmd->mask ? cmd->mask->id : 0, cmd->output->id);
#endif

   if (!cmd->input->w && !cmd->input->h
       && (cmd->mode != EVAS_FILTER_MODE_FILL))
     {
        XDBG("Skipping processing of empty input buffer (size 0x0)");
        return EINA_TRUE;
     }

   if ((cmd->output->w <= 0) || (cmd->output->h <= 0))
     {
        ERR("Output size invalid: %dx%d", cmd->output->w, cmd->output->h);
        return EINA_FALSE;
     }

   support = cmd->ENFN->gfx_filter_supports(cmd->ENDT, cmd);
   if (support == EVAS_FILTER_SUPPORT_NONE)
     {
        ERR("No function to process this filter (mode %d)", cmd->mode);
        return EINA_FALSE;
     }

   return cmd->ENFN->gfx_filter_process(cmd->ENDT, cmd);
}

static Eina_Bool
_filter_chain_run(Evas_Filter_Context *ctx)
{
   Evas_Filter_Command *cmd;
   Eina_Bool ok = EINA_FALSE;

   DEBUG_TIME_BEGIN();

   ctx->running = EINA_TRUE;
   EINA_INLIST_FOREACH(ctx->commands, cmd)
     {
        ok = _filter_command_run(cmd);
        if (!ok)
          {
             ERR("Filter processing failed!");
             goto end;
          }
     }

   ok = _filter_target_render(ctx);

end:
   ctx->running = EINA_FALSE;
   DEBUG_TIME_END();

   return ok;
}

static void
_filter_thread_run_cb(void *data)
{
   Evas_Filter_Context *ctx = data;
   Eina_Bool success;

   success = _filter_chain_run(ctx);

   if (ctx->post_run.cb)
     ctx->post_run.cb(ctx, ctx->post_run.data, success);
}

Eina_Bool
evas_filter_run(Evas_Filter_Context *ctx)
{
   Eina_Bool ret;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, EINA_FALSE);

   if (!ctx->commands)
     return EINA_TRUE;

   if (ctx->async)
     {
        evas_thread_queue_flush(_filter_thread_run_cb, ctx);
        return EINA_TRUE;
     }

   ret = _filter_chain_run(ctx);

   if (ctx->post_run.cb)
     ctx->post_run.cb(ctx, ctx->post_run.data, ret);
   return ret;
}


/* Logging */

static int init_cnt = 0;
int _evas_filter_log_dom = 0;

void
evas_filter_init(void)
{
   if ((init_cnt++) > 0) return;
   _evas_filter_log_dom = eina_log_domain_register("evas_filter", EVAS_FILTER_LOG_COLOR);
}

void
evas_filter_shutdown(void)
{
   if ((--init_cnt) > 0) return;
   evas_filter_parser_shutdown();
   eina_log_domain_unregister(_evas_filter_log_dom);
   _evas_filter_log_dom = 0;
}
